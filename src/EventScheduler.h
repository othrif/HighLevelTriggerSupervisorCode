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

      EventScheduler(monsvc::ptr<HLTSV> stats);
      EventScheduler(const EventScheduler& ) = delete;
      EventScheduler& operator=(const EventScheduler& ) = delete;
      
      ~EventScheduler();
      
      /**
       * \brief Add a DCM that has additional 'count' available cores. 
       *
       * For statistics also report the number of finished events.
       */
      void request_events(std::shared_ptr<DCMSession> dcm, unsigned int count = 1, unsigned int finished_events = 0);
      
      /** 
       * \brief Schedule an event to one of the available cores, after handling the re-assigned events.
       *
       * This method is called by the input thread reading from the RoIB.
       */
      void schedule_event(std::shared_ptr<LVL1Result> rois);
      
      /** 
       * \brief Re-assign an event if there was a problem with the DCM.
       *
       * This method is called from the DCMSession if there was a problem with the connection or a timeout.
       */
      void reassign_event(std::shared_ptr<LVL1Result> rois);

      /** 
       * \brief Reset the scheduler to the default state.
       *
       * Resets the global event ID and deletes all state about available DCMS.
       * This is called before a new run.
       */
      void reset(uint64_t initial_event_id);

      /** 
       * \brief Push all events in reassign queue to DCMs.
       *
       * This is called at the end of a run.
       */
      void push_events();

    private: // implementation

      // Internal routine to update the instantaneous rate.
      void update_instantaneous_rate();
      
      // The list of available DCM cores.
      tbb::concurrent_bounded_queue<std::weak_ptr<DCMSession>>   m_free_cores;
        
      // The list of re-assigned events.
      tbb::concurrent_bounded_queue<std::shared_ptr<LVL1Result>> m_reassigned_events; 

      // The global event ID.
      std::atomic<uint64_t> m_global_id;

      // The HLTSV statistics and counters.
      monsvc::ptr<HLTSV>    m_stats;

      // A flag to stop the rate update thread.
      bool                  m_update;

      // The rate update thread.
      std::thread           m_rate_thread;
    };
}

#endif // HLTSV_EVENTSCHEDULER_H_
