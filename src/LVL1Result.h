// this is -*- C++ -*-
#ifndef HLTSV_LVL1RESULT_H_
#define HLTSV_LVL1RESULT_H_

#include <chrono>

namespace hltsv {

    /** The ROIs received from RoIBuilder.
     * 
     * 
     */
    class LVL1Result {
    public:

        typedef std::chrono::high_resolution_clock  clock;
        typedef std::chrono::high_resolution_clock::time_point time_point;
        typedef std::chrono::high_resolution_clock::time_point::duration duration;

        LVL1Result(uint32_t *roi_data, size_t length);
        LVL1Result(uint32_t *roi_data[], size_t lengths[], size_t count);

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
    };
}

#endif // HLTSV_LVL1RESULT_H_
