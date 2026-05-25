#include "http_response.h"
#include "log.h"

void init_response(Http_response_t* res) {
    res->status_line = NULL;
    res->headers = NULL;
    res->trailers = NULL;
    res->has_body = 0;
    res->has_headers_hm = 0;
    res->has_trailers_hm = 0;
    res->send_chunked = 0;
}

void free_response(Http_response_t* res) {
    if (res->has_body) {
        free_body(&res->body);
    }
    if (res->has_headers_hm) {
        free(headers);
        free_field_line_hash_map(&res->headers_hm);
    }
    if (res->has_trailers_hm) {
        free(trailers);
        free_field_line_hash_map(&res->trailers_hm);
    }
}

Http_status_t prepare_response_for_valid_req_con(Http_response_t* res, Http_request_context_t* req_con) {

}

Http_status_t prepare_response_for_error(Http_response_t* res, Http_request_context_t* req_con) {
#define VERSION "HTTP/1.1"
#define SP " "
#define CRLF "\r\n"
#define case_GENERIC_RESPONSE(status_macro) case status_macro:\
res->status_line = VERSION SP stringify(status_macro) SP CRLF;\
res->headers = CRLF;\
break;
    switch (req_con->status) {
        case PARSING_BROKEN_CLOSE_CONNECTION:
            res->status_line = VERSION SP stringifyi(HTTP_STATUS_BAD_REQUEST) SP CRLF;
            res->headers = "Connection:close" CRLF CRLF;
            break;
        case_GENERIC_RESPONSE(HTTP_STATUS_BAD_REQUEST)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNAUTHORIZED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PAYMENT_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_FORBIDDEN)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_FOUND)
        case_GENERIC_RESPONSE(HTTP_STATUS_METHOD_NOT_ALLOWED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_ACCEPTABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_REQUEST_TIMEOUT)
        case_GENERIC_RESPONSE(HTTP_STATUS_CONFLICT)
        case_GENERIC_RESPONSE(HTTP_STATUS_GONE)
        case_GENERIC_RESPONSE(HTTP_STATUS_LENGTH_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PRECONDITION_FAILED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PAYLOAD_TOO_LARGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_URI_TOO_LONG)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE)
        case_GENERIC_RESPONSE(HTTP_STATUS_RANGE_NOT_SATISFIABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_EXPECTATION_FAILED)
        case_GENERIC_RESPONSE(HTTP_STATUS_I_M_A_TEAPOT)
        case_GENERIC_RESPONSE(HTTP_STATUS_MISDIRECTED_REQUEST)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNPROCESSABLE_ENTITY)
        case_GENERIC_RESPONSE(HTTP_STATUS_LOCKED)
        case_GENERIC_RESPONSE(HTTP_STATUS_FAILED_DEPENDANCY)
        case_GENERIC_RESPONSE(HTTP_STATUS_TOO_EARLY)
        case_GENERIC_RESPONSE(HTTP_STATUS_UPGRADE_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_PRECONDITION_REQUIRED)
        case_GENERIC_RESPONSE(HTTP_STATUS_TOO_MANY_REQUESTS)
        case_GENERIC_RESPONSE(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS)
        case_GENERIC_RESPONSE(HTTP_STATUS_INTERNAL_SERVER_ERROR)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_IMPLEMENTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_BAD_GATEWAY)
        case_GENERIC_RESPONSE(HTTP_STATUS_SERVICE_UNAVAILABLE)
        case_GENERIC_RESPONSE(HTTP_STATUS_GATEWAY_TIMEOUT)
        case_GENERIC_RESPONSE(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_VARIANT_ALSO_NEGOTIATES)
        case_GENERIC_RESPONSE(HTTP_STATUS_INSUFFICIENT_STORAGE)
        case_GENERIC_RESPONSE(HTTP_STATUS_LOOP_DETECTED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NOT_EXTENDED)
        case_GENERIC_RESPONSE(HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED)
    }
    return req_con->status;
#undef VERSION
#undef SP
#undef CRLF
#undef case_GENERIC_RESPONSE
}

Http_status_t prepare_response(Http_response_t* res, Http_request_context_t* req_con) {
    if (req_con->status == REQUEST_PROCESSING_FINE)
        return prepare_response_for_valid_req_con(res, req_con);

    return prepare_response_for_error(res, req_con);
}

void send_response(Http_response_t* res, Tcp_connection_t tcp_con) {

}

