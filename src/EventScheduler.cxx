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
        // m_stats->ProcessedEvents += finished_events;

        while(count-- > 0) {
	    m_free_cores.push(dcm);
        }

        {
            std::lock_guard<monsvc::ptr<HLTSV> > lock(m_stats);
            auto hltsv = m_stats.get();
            hltsv->ProcessedEvents += finished_events;
        }
    }


    // Schedule an event to one of the available cores, after handling
    // the re-assigned events.
    void EventScheduler::schedule_event(std::shared_ptr<LVL1Result> rois)
    {
        // There is only one input thread that calls this method, so
        // we don't protect the counter updates.
        auto hltsv = m_stats.get();
        hltsv->LVL1Events++;

        std::weak_ptr<DCMSession> dcm;
        std::shared_ptr<DCMSession> real_dcm;

        auto global_id = m_global_id++;
        rois->set_global_id(global_id);

        hltsv->Recent_Global_ID = global_id;
        hltsv->Recent_LVL1_ID = rois->l1_id();

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
		// m_stats->AssignedEvents++;
	    }
        } while(!real_dcm);        

        hltsv->AssignedEvents++;
    }

    void EventScheduler::push_events()
    {
        std::weak_ptr<DCMSession> dcm;
        std::shared_ptr<DCMSession> real_dcm;

        std::shared_ptr<LVL1Result> revent;

        unsigned int events = 0;
        while(m_reassigned_events.try_pop(revent)) {
            do {
                // might block
		m_free_cores.pop(dcm);
                real_dcm = dcm.lock();
                if(real_dcm) {
                    if(!real_dcm->handle_event(revent)) {
                        continue;
                    }
                    events++;
                }
            } while(!real_dcm);
        }

        if(events > 0) {
            auto hltsv = m_stats.get();
            hltsv->ReassignedEvents += events;;
        }

    }

    // Re-assign an event if there was a problem with the DCM.
    void EventScheduler::reassign_event(std::shared_ptr<LVL1Result> rois)
    {
        rois->set_reassigned();
        m_reassigned_events.push(rois);
    }

    void EventScheduler::reset(uint64_t initial_event_id)
    {
        m_global_id = initial_event_id;
        m_stats->reset();
        m_free_cores.clear();
        m_reassigned_events.clear();
    }

    void EventScheduler::update_instantaneous_rate()
    {
        uint64_t last_count = m_stats->ProcessedEvents;

        const int sleep_interval_secs = 2;

        while(m_update) {
            sleep(sleep_interval_secs);
            {
                auto n = m_free_cores.size();

                std::lock_guard<monsvc::ptr<HLTSV> > lock(m_stats);
                auto hltsv = m_stats.get();

                hltsv->AvailableCores = n < 0 ? 0 : n;

                if(hltsv->ProcessedEvents >= last_count)  {
                    auto rate   = (double)(hltsv->ProcessedEvents - last_count)/(double)(sleep_interval_secs);
                    hltsv->Rate = rate;
                }
                last_count    = hltsv->ProcessedEvents;

            }

        }
    }

}
