/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_PROTO_TX_RESPONSE_HPP
#define IROHA_PROTO_TX_RESPONSE_HPP

#include "backend/protobuf/transaction_responses/proto_concrete_tx_response.hpp"
#include "common/visitor.hpp"
#include "utils/lazy_initializer.hpp"
#include "utils/variant_deserializer.hpp"

namespace shared_model {
  namespace proto {
    /**
     * TransactionResponse is a status of transaction in system
     */
    class TransactionResponse final
        : public CopyableProto<interface::TransactionResponse,
                               iroha::protocol::ToriiResponse,
                               TransactionResponse> {
     public:
      /// Type of variant, that handle all concrete tx responses in the system
      using ProtoResponseVariantType = boost::variant<StatelessFailedTxResponse,
                                                      StatelessValidTxResponse,
                                                      StatefulFailedTxResponse,
                                                      StatefulValidTxResponse,
                                                      CommittedTxResponse,
                                                      MstExpiredResponse,
                                                      NotReceivedTxResponse,
                                                      MstPendingResponse,
                                                      MstPassedResponse>;

      /// Type with list of types in ResponseVariantType
      using ProtoResponseListType = ProtoResponseVariantType::types;

      template <typename TxResponse>
      explicit TransactionResponse(TxResponse &&ref)
          : CopyableProto(std::forward<TxResponse>(ref)) {}

      TransactionResponse(const TransactionResponse &r)
          : TransactionResponse(r.proto_) {}

      TransactionResponse(TransactionResponse &&r) noexcept
          : TransactionResponse(std::move(r.proto_)) {}

      const interface::types::HashType &transactionHash() const override {
        return *hash_;
      }

      /**
       * @return attached interface tx response
       */
      const ResponseVariantType &get() const override {
        return *ivariant_;
      }

      const ErrorMessageType &errorMessage() const override {
        return proto_->error_message();
      }

      /**
       * Compare priorities of two transaction responses
       * @param other response
       * @return:
       *    - -1, if this response's priority is less, than other's
       *    -  0, if it's equal
       *    -  1, if it's greater
       */
      int comparePriorities(const TransactionResponse &other) const noexcept {
        if (this->priority() < other.priority()) {
          return -1;
        } else if (this->priority() == other.priority()) {
          return 0;
        }
        return 1;
      };

     private:
      template <typename T>
      using Lazy = detail::LazyInitializer<T>;

      /// lazy variant shortcut
      using LazyVariantType = Lazy<ProtoResponseVariantType>;

      // lazy
      const LazyVariantType variant_{detail::makeLazyInitializer([this] {
        auto &&ar = *proto_;

        unsigned which = ar.GetDescriptor()
                             ->FindFieldByName("tx_status")
                             ->enum_type()
                             ->FindValueByNumber(ar.tx_status())
                             ->index();
        constexpr unsigned last =
            boost::mpl::size<ProtoResponseListType>::type::value - 1;

        return shared_model::detail::variant_impl<ProtoResponseListType>::
            template load<shared_model::proto::TransactionResponse::
                              ProtoResponseVariantType>(
                std::forward<decltype(ar)>(ar), which > last ? last : which);
      })};

      const Lazy<ResponseVariantType> ivariant_{detail::makeLazyInitializer(
          [this] { return ResponseVariantType(*variant_); })};

      // stub hash
      const Lazy<crypto::Hash> hash_{
          [this] { return crypto::Hash(this->proto_->tx_hash()); }};

      /**
       * @return priority this transaction response
       */
      int priority() const noexcept {
        return iroha::visit_in_place(
            *variant_,
            [](const StatelessValidTxResponse &) { return 1; },
            [](const MstPendingResponse &) { return 2; },
            [](const MstPassedResponse &) { return 3; },
            [](const StatefulValidTxResponse &) { return 4; },
            [](const CommittedTxResponse &) { return 5; },
            [](const StatelessFailedTxResponse &) { return 6; },
            [](const StatefulFailedTxResponse &) { return 7; },
            [](const MstExpiredResponse &) { return 8; },
            [](const NotReceivedTxResponse &) { return 9; });
      }
    };
  }  // namespace  proto
}  // namespace shared_model
#endif  // IROHA_PROTO_TX_RESPONSE_HPP
