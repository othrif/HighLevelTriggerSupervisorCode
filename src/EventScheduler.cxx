/* -*- c-basic-offset: 4 -*- */
#include "EventScheduler.h"
#include "DCMSession.h"

#include "ers/ers.h"
#include "LVL1Result.h"
#include "monsvc/MonitoringService.h"

namespace hltsv {

    EventScheduler::EventScheduler() :
        m_free_cores(),
        m_stats(monsvc::MonitoringService::instance().register_object("HLTSV",new HLTSV()))
    {
        m_global_id = 0;
    }


    EventScheduler::~EventScheduler()
    {
        // this should only be called when no more events are coming.

        // If there are any events in m_reassigned_events they have
        // to be pushed to the DCMs.
        push_events();

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

}
