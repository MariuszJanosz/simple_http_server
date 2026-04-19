#ifndef URI_H
#define URI_H

#include <ctype.h>

int uri_is_sub_delim(const char c);
int uri_is_gen_delim(const char c);
int uri_is_reserved(const char c);
int uri_is_unreserved(const char c);
int uri_is_pct_encoded(const char* s);
int uri_is_pchar(const char* s);
int uri_is_query(const char* s, int len);
int uri_is_fragment(const char* s, int len);
int uri_is_segment(const char* s, int len);
int uri_is_segment_nz(const char* s, int len);
int uri_is_segment_nz_nc(const char* s, int len);
int uri_is_path_empty(const char* s, int len);
int uri_is_path_rootless(const char* s, int len);
int uri_is_path_noscheme(const char* s, int len);
int uri_is_path_absolute(const char* s, int len);
int uri_is_path_abempty(const char* s, int len);
int uri_is_path(const char* s, int len);
int uri_is_reg_name(const char* s, int len);
int uri_is_dec_octet(const char* s, int len);
int uri_is_IPv4address(const char* s, int len);
int uri_is_ls32(const char* s, int len);
int uri_is_h16(const char* s, int len);
int uri_is_IPv6address(const char* s, int len);
int uri_is_IPvFuture(const char* s, int len);
int uri_is_IP_literal(const char* s, int len);
int uri_is_port(const char* s, int len);
int uri_is_host(const char* s, int len);
int uri_is_userinfo(const char* s, int len);
int uri_is_authority(const char* s, int len);
int uri_is_scheme(const char* s, int len);
int uri_is_relative_part(const char* s, int len);
int uri_is_relative_ref(const char* s, int len);
int uri_is_absolute_uri(const char* s, int len);
int uri_is_uri_reference(const char* s, int len);
int uri_is_hier_part(const char* s, int len);
int uri_is_uri(const char* s, int len);

#endif /*URI_H*/

