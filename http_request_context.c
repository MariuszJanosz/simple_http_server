#include "http_request_context.h"

void init_request_context(Http_request_context_t* req_con) {
    init_http_requst(&req_con->req);
    req_con->status = PARSING_FINE;
}

void free_request_context(Http_request_context_t* req_con) {
    free_http_request(&req_con->req);
}

void clean_request_context(Http_request_context_t* req_con) {
    clean_http_request(&req_con->req);
    req_con->status = PARSING_FINE;
}

Http_status_t process_request(Http_request_context_t* req_con) {
    //If there was an error before this phase, return immediately.
    if (req_con->status != PARINSG_FINE) return req_con->status;

    req_con->status = REQUEST_PROCESSING_FINE;

    //Populate the below array with processing stage functions in order those stages should run
    //if stage would return status different than REQUEST_PROCESSING_FINE
    //loop stops immediately and status of last processed stage is returned
    typedef Http_status_t (*processing_stage_func)(Http_request_context_t* req_con);
    const processing_stage_func processing_stages[] = {
        
    };

    for (   size_t i = 0;
            i < sizeof(processing_stages)/sizeof(processing_stages[0]) &&
            req_con->status == REQUEST_PROCESSING_FINE;
            ++i) {
        req_con->status = processing_stages[i](req_con);
    }
    return req_con->status;
}

