/*
 * Copyright (c) 2017 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_pipeline/receiver_endpoint.h
//! @brief Receiver endpoint pipeline.

#ifndef ROC_PIPELINE_RECEIVER_ENDPOINT_H_
#define ROC_PIPELINE_RECEIVER_ENDPOINT_H_

#include "roc_address/interface.h"
#include "roc_address/protocol.h"
#include "roc_core/iarena.h"
#include "roc_core/mpsc_queue.h"
#include "roc_core/optional.h"
#include "roc_core/ref_counted.h"
#include "roc_core/scoped_ptr.h"
#include "roc_packet/iparser.h"
#include "roc_packet/iwriter.h"
#include "roc_packet/shipper.h"
#include "roc_pipeline/config.h"
#include "roc_pipeline/state_tracker.h"
#include "roc_rtcp/composer.h"
#include "roc_rtcp/parser.h"
#include "roc_rtp/encoding_map.h"
#include "roc_rtp/parser.h"

namespace roc {
namespace pipeline {

class ReceiverSessionGroup;

//! Receiver endpoint sub-pipeline.
//!
//! Contains:
//!  - a pipeline for processing packets from single network endpoint
//!  - a reference to session group to which packets are routed
class ReceiverEndpoint : public core::RefCounted<ReceiverEndpoint, core::ArenaAllocation>,
                         public core::ListNode,
                         private packet::IWriter {
public:
    //! Initialize.
    ReceiverEndpoint(address::Protocol proto,
                     StateTracker& state_tracker,
                     ReceiverSessionGroup& session_group,
                     const rtp::EncodingMap& encoding_map,
                     const address::SocketAddr& inbound_address,
                     packet::IWriter* outbound_writer,
                     core::IArena& arena);

    //! Check if the port pipeline was succefully constructed.
    bool is_valid() const;

    //! Get protocol.
    address::Protocol proto() const;

    //! Get composer for outbound packets.
    //! @remarks
    //!  This composer will create packets according to endpoint protocol.
    //! @note
    //!  Not all protocols support outbound packets on receiver. If it's
    //!  not supported, the method returns NULL.
    packet::IComposer* outbound_composer();

    //! Get writer for outbound packets.
    //! This way feedback packets for sender generated by receiver reach network.
    //! @remarks
    //!  Packets passed to this writer will be enqueued for sending.
    //!  When frame is requested to ReceiverSession, it generates packets
    //!  and writes them to outbound writers of endpoints.
    //! @note
    //!  Not all protocols support outbound packets on receiver. If it's
    //!  not supported, the method returns NULL.
    packet::IWriter* outbound_writer();

    //! Get bind address for inbound packets.
    const address::SocketAddr& inbound_address() const;

    //! Get writer for inbound packets.
    //! This way packets from network reach receiver pipeline.
    //! @remarks
    //!  Packets passed to this writer will be pulled into pipeline.
    //!  This writer is thread-safe and lock-free, packets can be written
    //!  to it from netio thread.
    packet::IWriter& inbound_writer();

    //! Pull packets written to inbound writer into pipeline.
    //! @remarks
    //!  Packets are written to inbound_writer() from network thread.
    //!  They don't appear in pipeline immediately. Instead, pipeline thread
    //!  should periodically call pull_packets() to make them available.
    ROC_ATTR_NODISCARD status::StatusCode pull_packets(core::nanoseconds_t current_time);

private:
    virtual ROC_ATTR_NODISCARD status::StatusCode write(const packet::PacketPtr& packet);

    const address::Protocol proto_;

    StateTracker& state_tracker_;
    ReceiverSessionGroup& session_group_;

    // Outbound packets sub-pipeline.
    // On receiver, typically present only in control endpoints.
    packet::IComposer* composer_;
    core::Optional<rtcp::Composer> rtcp_composer_;
    core::Optional<packet::Shipper> shipper_;

    // Inbound packets sub-pipeline.
    // On receiver, always present.
    packet::IParser* parser_;
    core::Optional<rtp::Parser> rtp_parser_;
    core::ScopedPtr<packet::IParser> fec_parser_;
    core::Optional<rtcp::Parser> rtcp_parser_;
    address::SocketAddr inbound_address_;
    core::MpscQueue<packet::Packet> inbound_queue_;

    bool valid_;
};

} // namespace pipeline
} // namespace roc

#endif // ROC_PIPELINE_RECEIVER_ENDPOINT_H_
