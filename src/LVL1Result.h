// this is -*- C++ -*-
#ifndef HLTSV_LVL1RESULT_H_
#define HLTSV_LVL1RESULT_H_

#include <chrono>
#include <memory>
#include <functional>
#include <vector>

namespace hltsv {

    /** The ROIs received from RoIBuilder.
     * 
     * Internally the ROIs are kept as a vector of
     * raw pointers to 4byte words. Memory management is
     * defined by the deleter that is passed to the constructor
     * and which is called for each ROI element.
     *
     * The default is to assume the data is a dynamically allocated uint32_t[] type 
     * that is deleted with 'delete [] data'.
     */
    class LVL1Result {
    public:

        typedef std::chrono::high_resolution_clock  clock;
        typedef std::chrono::high_resolution_clock::time_point time_point;
        typedef std::chrono::high_resolution_clock::time_point::duration duration;

        // Constructor for single buffer.
        template<class DELETER = std::default_delete<uint32_t[]>>
        LVL1Result(uint32_t *roi_data, uint32_t length, DELETER del = DELETER())
            : m_data(1, roi_data),
              m_lengths(1, length),
              m_deleter(del),
              m_reassigned(false)
        {
        }

        // Constructor for multiple buffers.
        template<class DELETER = std::default_delete<uint32_t[]>>
        LVL1Result(uint32_t count, uint32_t *roi_data[], uint32_t lengths[], DELETER del = DELETER())
            : m_deleter(del),
              m_reassigned(false)
        {
            m_data.assign(&roi_data[0], &roi_data[count]);
            m_lengths.assign(&lengths[0], &lengths[count]);
        }

        ~LVL1Result();

        LVL1Result(const LVL1Result& ) = delete;
        LVL1Result& operator=(const LVL1Result& ) = delete;

        uint32_t l1_id() const;
        bool     reassigned() const;
        void     set_reassign();

        time_point timestamp() const;
        void       set_timestamp(time_point ts = clock::now());

        template<class ConstBufferSequence>
        void insert(ConstBufferSequence& buffers) const
        {
            // add the internal representation to the ASIO ConstBufferSequence 'buffers'.
        }

    private:
        std::vector<uint32_t*>          m_data;
        std::vector<uint32_t>           m_lengths;
        std::function<void (uint32_t*)> m_deleter;
        time_point                      m_time_stamp;
        bool                            m_reassigned;
    };
}

#endif // HLTSV_LVL1RESULT_H_
