/* -*- c-basic-offset: 4 -*- */
#include "EventScheduler.h"
#include "DCMSession.h"

#include "ers/ers.h"
#include "LVL1Result.h"

namespace hltsv {

    EventScheduler::EventScheduler() :
        m_free_cores()
    {
        ERS_LOG("EventScheduler::EventScheduler()" << m_free_cores.size());
        m_global_id = 0;
    }


    EventScheduler::~EventScheduler()
    {
        // this should only be called when no more events are coming.

        // If there are any events in m_reassigned_events they have
        // to be pushed to the DCMs.

    }

    // Add a DCM that has additional 'count' available cores.
    void EventScheduler::request_events(std::shared_ptr<DCMSession> dcm, unsigned int count)
    {
        ERS_LOG("EventScheduler::request_events, with count=" << count );

        while(count-- > 0) {
            m_free_cores.push(dcm);
        }
        ERS_LOG("EventScheduler::request_events,m_free_cores=" << m_free_cores.size() );
    }


    // Schedule an event to one of the available cores, after handling
    // the re-assigned events.
    void EventScheduler::schedule_event(std::shared_ptr<LVL1Result> rois)
    {
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
            }
        } while(!real_dcm);        
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
                }
            } while(!real_dcm);
        }

    }

    // Re-assign an event if there was a problem with the DCM.
    void EventScheduler::reassign_event(std::shared_ptr<LVL1Result> rois)
    {
        m_reassigned_events.push(rois);
    }

    void EventScheduler::reset()
    {
        m_global_id = 0;
    }

}
