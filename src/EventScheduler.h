// this is -*- C++ -*-
#ifndef HLTSV_EVENTSCHEDULER_H_
#define HLTSV_EVENTSCHEDULER_H_

#include <memory>
#include "tbb/concurrent_queue.h"

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
      
      // Add a DCM that has additional 'count' available cores.
      void request_events(std::shared_ptr<DCMSession> dcm, unsigned int count = 1);
      
      // Schedule an event to one of the available cores, after handling
      // the re-assigned events.
      void schedule_event(std::shared_ptr<LVL1Result> rois);
      
      // Re-assign an event if there was a problem with the DCM.
      void reassign_event(std::shared_ptr<LVL1Result> rois);
      
    private: // implementation
      
      // push events in reassign queue to DCMs
      void push_events();
      
      tbb::concurrent_bounded_queue<std::weak_ptr<DCMSession>>   m_free_cores;
      // maybe unique pointer better ? 
      tbb::concurrent_bounded_queue<std::shared_ptr<LVL1Result>> m_reassigned_events; 
    };
}

#endif // HLTSV_EVENTSCHEDULER_H_
