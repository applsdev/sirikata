// Copyright (c) 2012 Sirikata Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _SIRIKATA_PRESENCE_PROPERTIES_LOC_UPDATE_HPP_
#define _SIRIKATA_PRESENCE_PROPERTIES_LOC_UPDATE_HPP_

#include <sirikata/pintoloc/Platform.hpp>
#include <sirikata/pintoloc/LocUpdate.hpp>

namespace Sirikata {

/** Implementation of LocUpdate which collects its information from
 *  stored SequencedPresenceProperties.
 */
class PresencePropertiesLocUpdate : public LocUpdate {
public:
    /** Construct a PresencePropertiesLocUpdate from a
     *  SequencedPresenceProperties. Note that this assumes the times are
     *  already local.
     */
    PresencePropertiesLocUpdate(const ObjectReference& o, const SequencedPresenceProperties& lu)
     : mObject(o),
       mUpdate(lu)
    {}
    virtual ~PresencePropertiesLocUpdate() {}

    virtual ObjectReference object() const { return mObject; }

    // Request epoch
    // Since these are only used for orphan updates, we should never have an
    // epoch we would care about -- presences should never have orphans since
    // their proxies are created as soon as they get connected.
    virtual bool has_epoch() const { return false; }
    virtual uint64 epoch() const { return 0; }

    // Parent aggregate
    virtual bool has_parent() const { return (parent() != ObjectReference::null()); }
    virtual ObjectReference parent() const { return ObjectReference(mUpdate.parent()); }
    virtual uint64 parent_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_PARENT_PART); }

    // Location
    virtual bool has_location() const { return true; }
    virtual TimedMotionVector3f location() const { return mUpdate.location(); }
    virtual uint64 location_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_POS_PART); }

    // Orientation
    virtual bool has_orientation() const { return true; }
    virtual TimedMotionQuaternion orientation() const { return mUpdate.orientation(); }
    virtual uint64 orientation_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_ORIENT_PART); }

    // Bounds
    virtual bool has_bounds() const { return true; }
    virtual AggregateBoundingInfo bounds() const { return mUpdate.bounds(); }
    virtual uint64 bounds_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_BOUNDS_PART); }

    // Mesh
    virtual bool has_mesh() const { return true; }
    virtual String mesh() const { return mUpdate.mesh().toString(); }
    virtual uint64 mesh_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_MESH_PART); }

    // Physics
    virtual bool has_physics() const { return true; }
    virtual String physics() const { return mUpdate.physics(); }
    virtual uint64 physics_seqno() const { return mUpdate.getUpdateSeqNo(SequencedPresenceProperties::LOC_PHYSICS_PART); }

    // Prox IndexIDs
    virtual uint32 index_id_size() const { return 0; }
    virtual ProxIndexID index_id(int32 idx) const { assert(false && "Out of bounds index id"); return -1; }
    virtual uint64 index_id_seqno() const { assert(false && "PresencePropertiesLocUpdate doesn't have index ID support."); }

private:
    PresencePropertiesLocUpdate();
    PresencePropertiesLocUpdate(const PresencePropertiesLocUpdate&);

    const ObjectReference& mObject;
    const SequencedPresenceProperties& mUpdate;
};

/** Implementation of LocUpdate which collects its information from
 *  stored SequencedPresenceProperties and optionally provides an epoch. Useful
 *  for updates where you want to expose epochs (e.g. because you cache results
 *  and then forward results to local queriers), but epochs are tracked
 *  separately.
 */
class PresencePropertiesLocUpdateWithEpoch : public PresencePropertiesLocUpdate {
public:
    /** Construct a PresencePropertiesLocUpdate from a
     *  SequencedPresenceProperties. Note that this assumes the times are
     *  already local.
     */
    PresencePropertiesLocUpdateWithEpoch(const ObjectReference& o, const SequencedPresenceProperties& lu, bool has_ep, uint64 ep)
     : PresencePropertiesLocUpdate(o, lu),
       mHasEpoch(has_ep),
       mEpoch(ep)
    {}
    virtual ~PresencePropertiesLocUpdateWithEpoch() {}

    virtual bool has_epoch() const { return mHasEpoch; }
    virtual uint64 epoch() const { return mEpoch; }

private:
    const bool mHasEpoch;
    const uint64 mEpoch;
};

} // namespace Sirikata

#endif //_SIRIKATA_PRESENCE_PROPERTIES_LOC_UPDATE_HPP_
