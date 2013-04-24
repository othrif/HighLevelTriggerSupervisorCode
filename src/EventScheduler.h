// this is -*- C++ -*-
#ifndef HLTSV_EVENTSCHEDULER_H_
#define HLTSV_EVENTSCHEDULER_H_

#include <atomic>
#include <memory>
#include <thread>

#include "tbb/concurrent_queue.h"

#include "monsvc/ptr.h"
#include "HLTSV.h"

namespace hltsv {

    class DCMSession;
    class LVL1Result;

    /** Schedule events onto DCMs.
    *
    * The EventScheduler is a passive object that is called either
    * in the context of the RoI input thread (schedule_event) or
    * the DCM that is ready to handle more events (request_events).
    *
    * It also deals with re-assigned events that are passed back to
    * him from DCMs that could not process an event.
    */
    class EventScheduler {
    public:

      EventScheduler();
      EventScheduler(const EventScheduler& ) = delete;
      EventScheduler& operator=(const EventScheduler& ) = delete;
      
      ~EventScheduler();
      
      // Add a DCM that has additional 'count' available cores. For statistics also report the number of finished events.
      void request_events(std::shared_ptr<DCMSession> dcm, unsigned int count = 1, unsigned int finished_events = 0);
      
      // Schedule an event to one of the available cores, after handling
      // the re-assigned events.
      void schedule_event(std::shared_ptr<LVL1Result> rois);
      
      // Re-assign an event if there was a problem with the DCM.
      void reassign_event(std::shared_ptr<LVL1Result> rois);

     // Reset the global event ID to 0
      void reset();
      
    private: // implementation
      
      // push events in reassign queue to DCMs
      void push_events();

      void update_instantaneous_rate();
      
      tbb::concurrent_bounded_queue<std::weak_ptr<DCMSession>>   m_free_cores;
      tbb::concurrent_bounded_queue<std::shared_ptr<LVL1Result>> m_reassigned_events; 

      std::atomic<uint64_t> m_global_id;
      monsvc::ptr<HLTSV>    m_stats;

      bool                  m_update;
      std::thread           m_rate_thread;
    };
}

#endif // HLTSV_EVENTSCHEDULER_H_
