/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_connection_manager.hpp"

#include <boost/range/combine.hpp>
#include "interfaces/iroha_internal/proposal.hpp"

using namespace iroha::ordering;

OnDemandConnectionManager::OnDemandConnectionManager(
    std::shared_ptr<transport::OdOsNotificationFactory> factory,
    CurrentPeers initial_peers,
    rxcpp::observable<CurrentPeers> peers)
    : factory_(std::move(factory)),
      subscription_(peers.subscribe([this](const auto &peers) {
        // exclusive lock
        std::lock_guard<std::shared_timed_mutex> lock(mutex_);

        this->initializeConnections(peers);
      })) {
  // using start_with(initial_peers) results in deadlock
  initializeConnections(initial_peers);
}

void OnDemandConnectionManager::onTransactions(transport::RoundType round,
                                               CollectionType transactions) {
  // shared lock
  std::shared_lock<std::shared_timed_mutex> lock(mutex_);

  const PeerType types[] = {kCurrentRoundRejectConsumer,
                        kNextRoundRejectConsumer,
                        kNextRoundCommitConsumer};
  const transport::RoundType rounds[] = {{round.first, round.second + 2},
                                         {round.first + 1, 2},
                                         {round.first + 2, 1}};

  for (auto &&pair : boost::combine(types, rounds)) {
    connections_.peers[boost::get<0>(pair)]->onTransactions(boost::get<1>(pair),
                                                            transactions);
  }
}

boost::optional<OnDemandConnectionManager::ProposalType>
OnDemandConnectionManager::onRequestProposal(transport::RoundType round) {
  // shared lock
  std::shared_lock<std::shared_timed_mutex> lock(mutex_);

  return connections_.peers[kIssuer]->onRequestProposal(round);
}

void OnDemandConnectionManager::initializeConnections(
    const CurrentPeers &peers) {
  auto create_assign = [this](auto &ptr, auto &peer) {
    ptr = factory_->create(*peer);
  };

  for (auto pair : boost::combine(connections_.peers, peers.peers)) {
    create_assign(boost::get<0>(pair), boost::get<1>(pair));
  }
}
