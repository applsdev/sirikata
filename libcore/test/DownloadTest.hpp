/*  Sirikata Transfer -- Content Distribution Network
 *  DownloadTest.hpp
 *
 *  Copyright (c) 2009, Patrick Reiter Horn
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
 *  * Neither the name of Sirikata nor the names of its contributors may
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
/*  Created on: Feb 17, 2009 */

#include <cxxtest/TestSuite.h>
#include "transfer/EventTransferManager.hpp"
#include "task/EventManager.hpp"
#include "transfer/CachedServiceLookup.hpp"
#include "transfer/CachedNameLookupManager.hpp"
#include "transfer/NetworkCacheLayer.hpp"
#include "transfer/MemoryCacheLayer.hpp"
#include "transfer/HTTPDownloadHandler.hpp"
#include "transfer/URI.hpp"
#include "transfer/LRUPolicy.hpp"


using namespace Sirikata;
class DownloadTest : public CxxTest::TestSuite {
	typedef Transfer::TransferManager TransferManager;
	typedef Transfer::NameLookupManager NameLookupManager;
	typedef Transfer::CacheLayer CacheLayer;
	typedef Transfer::URI URI;
	typedef Transfer::Range Range;
	typedef Transfer::URIContext URIContext;

	TransferManager *mTransferManager;
	TransferManager *mCachedTransferManager;
	CacheLayer *mMemoryCache;
	CacheLayer *mCachedNetworkCache;
	// No disk cache because it doesn't need to be persistent.
	CacheLayer *mNetworkCache;
	NameLookupManager *mNameLookup;
	NameLookupManager *mCachedNameLookup;
	Transfer::ProtocolRegistry<Transfer::DownloadHandler> *mDownloadReg;
	Transfer::ProtocolRegistry<Transfer::NameLookupHandler> *mNameLookupReg;
	Transfer::ServiceLookup *mService;

	Task::GenEventManager *mEventSystem;

	boost::thread *mEventProcessThread;

	int finishedTest;
	boost::mutex wakeMutex;
	boost::condition_variable wakeCV;


public:

	static Task::EventResponse printTransfer(Task::EventPtr evptr) {
		Transfer::DownloadEventPtr ev (std::tr1::dynamic_pointer_cast<Transfer::DownloadEvent>(evptr));

		std::cout << "Transfer " << (ev->success()?"finished":"failed")<<
			" (" << ev->mStatus << "): " << ev->uri();
		Range::printRangeList(std::cout, ev->data());
		std::cout << std::endl;

		return Task::EventResponse::nop();
	}

	void setUp() {
		mEventSystem = new Task::GenEventManager(true);
		mEventProcessThread = new boost::thread(boost::bind(
			&Task::GenEventManager::sleep_processEventQueue, mEventSystem));

		mService = new Transfer::CachedServiceLookup;

		Transfer::ListOfServices *services;
		services = new Transfer::ListOfServices;
		services->push_back(URIContext("http","graphics.stanford.edu","","~danielrh/dns/names/global"));
		mService->addToCache(URIContext("meerkat","","",""), Transfer::ListOfServicesPtr(services));

		services = new Transfer::ListOfServices;
		services->push_back(URIContext("http","graphics.stanford.edu","","~danielrh/uploadsystem/files/global"));
		mService->addToCache(URIContext("mhash","","",""), Transfer::ListOfServicesPtr(services));

		mNameLookupReg = new Transfer::ProtocolRegistry<Transfer::NameLookupHandler>;
		boost::shared_ptr<Transfer::HTTPDownloadHandler> httpHandler(new Transfer::HTTPDownloadHandler);
		mNameLookupReg->setHandler("http", httpHandler);
		mNameLookup = new Transfer::NameLookupManager(mService, mNameLookupReg);

		mDownloadReg = new Transfer::ProtocolRegistry<Transfer::DownloadHandler>;
		mDownloadReg->setHandler("http", httpHandler);
		mNetworkCache = new Transfer::NetworkCacheLayer(NULL, mService, mDownloadReg);

		mTransferManager = new Transfer::EventTransferManager(mNetworkCache, mNameLookup, mEventSystem);

		// Uses the same event system, so don't combine the cached and non-cached ones into a single test.
		mCachedNetworkCache = new Transfer::NetworkCacheLayer(NULL, mService, mDownloadReg);
		mMemoryCache = new Transfer::MemoryCacheLayer(new Transfer::LRUPolicy(1000000), mCachedNetworkCache);
		mCachedNameLookup = new Transfer::CachedNameLookupManager(mService, mNameLookupReg);
		mCachedTransferManager = new Transfer::EventTransferManager(mMemoryCache, mCachedNameLookup, mEventSystem);

		mEventSystem->subscribe(Transfer::DownloadEventId, &printTransfer, Task::EARLY);

		finishedTest = 0;
	}
	void tearDown() {
		delete mTransferManager;
		delete mNetworkCache;
		delete mMemoryCache;
		delete mCachedNetworkCache;
		delete mDownloadReg;
		delete mNameLookup;
		delete mCachedNameLookup;
		delete mNameLookupReg;
		delete mService;
		delete mEventSystem;
		delete mEventProcessThread;

		finishedTest = 0;
	}
	void waitFor(int numTests) {
		boost::unique_lock<boost::mutex> wakeup(wakeMutex);
		while (finishedTest < numTests) {
			wakeCV.wait(wakeup);
		}
	}
	void notifyOne() {
		boost::unique_lock<boost::mutex> wakeup(wakeMutex);
		finishedTest++;
		wakeCV.notify_one();
	}

	Task::EventResponse downloadFinished(Task::EventPtr evbase) {
		Transfer::DownloadEventPtr ev = std::tr1::dynamic_pointer_cast<Transfer::DownloadEvent> (evbase);

		TS_ASSERT_EQUALS(ev->mStatus, Transfer::TransferManager::SUCCESS);

		notifyOne();

		return Task::EventResponse::del();
	}

	void testSimpleFileDownload() {
		mTransferManager->download("meerkat:///arcade.mesh",
			boost::bind(&DownloadTest::downloadFinished, this, _1), Range(true));
		mTransferManager->download("meerkat:///arcade.mesh",
			boost::bind(&DownloadTest::downloadFinished, this, _1), Range(true));
		waitFor(2);
	}

	void brokentestCombiningRangedFileDownload() {
		mTransferManager->download("meerkat:///arcade.mesh",
			boost::bind(&DownloadTest::downloadFinished, this, _1), Range(0,3, Transfer::BOUNDS));
		mTransferManager->download("meerkat:///arcade.mesh",
			boost::bind(&DownloadTest::downloadFinished, this, _1), Range(true));
		mTransferManager->download("meerkat:///arcade.mesh",
			boost::bind(&DownloadTest::downloadFinished, this, _1), Range(2));
		waitFor(2);
	}
};
