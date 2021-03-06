/*  proxsim
 *  PintoManagerLocationServiceCache.cpp
 *
 *  Copyright (c) 2009, Ewen Cheslack-Postava
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of libprox nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PintoManagerLocationServiceCache.hpp"

#define PINTO_LOG(lvl, msg) SILOG(pinto,lvl,msg)

namespace Sirikata {

typedef Prox::LocationServiceCache<ServerProxSimulationTraits> LocationServiceCache;

PintoManagerLocationServiceCache::PintoManagerLocationServiceCache() {
}

PintoManagerLocationServiceCache::~PintoManagerLocationServiceCache() {
}

void PintoManagerLocationServiceCache::addSpaceServer(ServerID sid, const TimedMotionVector3f& loc, const BoundingSphere3f& region, float32 ms) {
    Lock lck(mMutex);

    bool alreadyHad = (mServers.find(sid) != mServers.end());

    SpaceServerData old_data = mServers[sid];

    mServers[sid].location = loc;
    mServers[sid].region = region;
    mServers[sid].maxSize = ms;
    mServers[sid].aggregate = false;
    if (!alreadyHad) {
        mServers[sid].tracking = false;
        mServers[sid].removable = true;
    }

    if (!alreadyHad) {
        for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++)
            (*it)->locationConnected(sid, false, false, loc, region, ms);
    }
    else {
        for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++) {
            (*it)->locationPositionUpdated(sid, old_data.location, loc);
            (*it)->locationRegionUpdated(sid, old_data.region, region);
            (*it)->locationMaxSizeUpdated(sid, old_data.maxSize, ms);
        }
    }
}

void PintoManagerLocationServiceCache::updateSpaceServerLocation(ServerID sid, const TimedMotionVector3f& loc) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    TimedMotionVector3f old_loc = dat.location;
    dat.location = loc;
    for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++)
        (*it)->locationPositionUpdated(sid, old_loc, loc);
}

void PintoManagerLocationServiceCache::updateSpaceServerRegion(ServerID sid, const BoundingSphere3f& region) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    BoundingSphere3f old_region = dat.region;
    dat.region = region;
    for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++)
        (*it)->locationRegionUpdated(sid, old_region, region);
}

void PintoManagerLocationServiceCache::updateSpaceServerMaxSize(ServerID sid, float32 ms) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    float32 old_ms = dat.maxSize;
    dat.maxSize = ms;
    for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++)
        (*it)->locationMaxSizeUpdated(sid, old_ms, ms);
}

void PintoManagerLocationServiceCache::removeSpaceServer(ServerID sid) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    if (dat.tracking) {
        dat.removable = true;
        return;
    }

    mServers.erase(it);

    for(ListenerSet::iterator it = mListeners.begin(); it != mListeners.end(); it++)
        (*it)->locationDisconnected(sid);
}

void PintoManagerLocationServiceCache::addAggregate(ServerID sid) {
    Lock lck(mMutex);

    assert(mServers.find(sid) == mServers.end());

    SpaceServerData old_data = mServers[sid];

    // Dummy info, we'll get updates soon.
    mServers[sid].location = TimedMotionVector3f(Time::null(), MotionVector3f());
    mServers[sid].region = BoundingSphere3f();
    mServers[sid].maxSize = 0;
    mServers[sid].aggregate = true;
    mServers[sid].tracking = 0;
    mServers[sid].removable = false; // Removal only by removeAggregate
}

void PintoManagerLocationServiceCache::updateAggregateLocation(ServerID sid, const TimedMotionVector3f& loc) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    dat.location = loc;
}

void PintoManagerLocationServiceCache::updateAggregateBounds(ServerID sid, const AggregateBoundingInfo& bnds) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    dat.region = bnds.centerBounds();
    dat.maxSize = bnds.maxObjectRadius;
}

void PintoManagerLocationServiceCache::removeAggregate(ServerID sid) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(sid);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    // Nothing should be tracking since aggregates were generated by the query
    // processor and we never let anyone know about them.
    assert(dat.tracking == 0);
    mServers.erase(it);
}

void PintoManagerLocationServiceCache::addPlaceholderImposter(
    const ObjectID& uuid,
    const Vector3f& center_offset,
    const float32 center_bounds_radius,
    const float32 max_size,
    const String& zernike,
    const String& mesh
) {
    // We might get calls here if we have an aggregate listener registered, but
    // we don't really care about it. We'll just ignore the call.
}

LocationServiceCache::Iterator PintoManagerLocationServiceCache::startTracking(const ObjectID& id) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(id);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    dat.tracking++;

    return Iterator( new ServerMap::iterator(it) );
}

#define EXTRACT_ITERATOR(x) (*((ServerMap::iterator*)x.data))
#define EXTRACT_ITERATOR_DATA(x) (EXTRACT_ITERATOR(x)->second)

void PintoManagerLocationServiceCache::stopTracking(const Iterator& id) {
    Lock lck(mMutex);

    assert(id.data != NULL);
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    dat.tracking--;

    if (dat.tracking == 0 && dat.removable)
        mServers.erase( EXTRACT_ITERATOR(id) );
}

bool PintoManagerLocationServiceCache::startRefcountTracking(const ObjectID& id) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(id);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;

    dat.tracking++;

    return true;
}

void PintoManagerLocationServiceCache::stopRefcountTracking(const ObjectID& id) {
    Lock lck(mMutex);

    ServerMap::iterator it = mServers.find(id);
    assert( it != mServers.end() );
    SpaceServerData& dat = it->second;
    dat.tracking--;

    if (dat.tracking == 0 && dat.removable)
        mServers.erase(it);
}

bool PintoManagerLocationServiceCache::tracking(const ObjectID& id) {
    Lock lck(mMutex);

    return (mServers.find(id) != mServers.end());
}


TimedMotionVector3f PintoManagerLocationServiceCache::location(const Iterator& id) {
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    return dat.location;
}

Vector3f PintoManagerLocationServiceCache::centerOffset(const Iterator& id) {
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    return dat.region.center();
}

float32 PintoManagerLocationServiceCache::centerBoundsRadius(const Iterator& id) {
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    return dat.region.radius();
}

float32 PintoManagerLocationServiceCache::maxSize(const Iterator& id) {
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    return dat.maxSize;
}

Prox::ZernikeDescriptor& PintoManagerLocationServiceCache::zernikeDescriptor(const Iterator& i) {
  return Prox::ZernikeDescriptor::null();
}

String PintoManagerLocationServiceCache::mesh(const Iterator& i) {
  return String("");
}

bool PintoManagerLocationServiceCache::isLocal(const Iterator& id) {
    // non-sensical here
    return false;
}

bool PintoManagerLocationServiceCache::aggregate(const Iterator& id) {
    SpaceServerData& dat = EXTRACT_ITERATOR_DATA(id);
    return dat.aggregate;
}

const ServerID& PintoManagerLocationServiceCache::iteratorID(const Iterator& id) {
    return EXTRACT_ITERATOR(id)->first;
}

void PintoManagerLocationServiceCache::addUpdateListener(LocationUpdateListenerType* listener) {
    Lock lck(mMutex);

    assert( mListeners.find(listener) == mListeners.end() );
    mListeners.insert(listener);
}

void PintoManagerLocationServiceCache::removeUpdateListener(LocationUpdateListenerType* listener) {
    Lock lck(mMutex);

    ListenerSet::iterator it = mListeners.find(listener);
    assert( it != mListeners.end() );
    mListeners.erase(it);
}

} // namespace Sirikata
