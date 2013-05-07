/* -*- c-basic-offset: 4 -*- */
#include "EventScheduler.h"
#include "DCMSession.h"

#include "ers/ers.h"
#include "LVL1Result.h"
#include "monsvc/MonitoringService.h"

namespace hltsv {

    EventScheduler::EventScheduler() :
        m_free_cores(),
        m_stats(monsvc::MonitoringService::instance().register_object("Events",new HLTSV(), true)),
        m_update(true),
        m_rate_thread(&EventScheduler::update_instantaneous_rate, this)
    {
        m_global_id = 0;
        
    }


    EventScheduler::~EventScheduler()
    {
        m_free_cores.abort();
        m_reassigned_events.abort();

        m_update = false;
        m_rate_thread.join();

        monsvc::MonitoringService::instance().remove_object(std::string("Events"));

    }

    // Add a DCM that has additional 'count' available cores.
    void EventScheduler::request_events(std::shared_ptr<DCMSession> dcm, unsigned int count, unsigned int finished_events)
    {
        ERS_DEBUG(1,"EventScheduler::request_events, with count = " << count );
        m_stats->ProcessedEvents += finished_events;

        while(count-- > 0) {
            m_free_cores.push(dcm);
        }
        m_stats->AvailableCores = m_free_cores.size();
    }


    // Schedule an event to one of the available cores, after handling
    // the re-assigned events.
    void EventScheduler::schedule_event(std::shared_ptr<LVL1Result> rois)
    {

        m_stats->LVL1Events++;

        std::weak_ptr<DCMSession> dcm;
        std::shared_ptr<DCMSession> real_dcm;

        rois->set_global_id(m_global_id++);

        // First try to work on the re-assigned events if there are any
        push_events();

        // now handle the new event
        do {
            // might block
            m_free_cores.pop(dcm);
            real_dcm = dcm.lock();
            if(real_dcm) {
                if(!real_dcm->handle_event(rois)) {
                    continue;
                }
                m_stats->AssignedEvents++;
            }
        } while(!real_dcm);        

        m_stats->AvailableCores = m_free_cores.size();
    }

    void EventScheduler::push_events()
    {
        std::weak_ptr<DCMSession> dcm;
        std::shared_ptr<DCMSession> real_dcm;

        std::shared_ptr<LVL1Result> revent;
        while(m_reassigned_events.try_pop(revent)) {
            do {
                // might block
                m_free_cores.pop(dcm);
                real_dcm = dcm.lock();
                if(real_dcm) {
                    if(!real_dcm->handle_event(revent)) {
                        continue;
                    }
                    m_stats->ReassignedEvents++;
                }
            } while(!real_dcm);
        }

    }

    // Re-assign an event if there was a problem with the DCM.
    void EventScheduler::reassign_event(std::shared_ptr<LVL1Result> rois)
    {
        rois->set_reassigned();
        m_reassigned_events.push(rois);
    }

    void EventScheduler::reset()
    {
        m_global_id = 0;
        m_stats->reset();
    }

    void EventScheduler::update_instantaneous_rate()
    {
        uint64_t last_count = m_stats->ProcessedEvents;

        while(m_update) {
            sleep(5);
            auto rate     = (double)(m_stats->ProcessedEvents - last_count)/5.0;
            m_stats->Rate = rate;
            last_count    = m_stats->ProcessedEvents;
        }
    }

}
