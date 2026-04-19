#include "abnf.h"
#include "uri.h"

/* rev\ denotes / in block comments, otherwise / might break them */

int uri_is_sub_delim(const char c) {
    return  0x21 == c /*!*/|| 0x24 == c /*$*/||
            0x26 == c /*&*/|| 0x60 == c /*'*/||
            0x28 == c /*(*/|| 0x29 == c /*)*/||
            0x2A == c /***/|| 0x2B == c /*+*/||
            0x2C == c /*,*/|| 0x3B == c /*;*/||
            0x3D == c /*=*/;
}

int uri_is_gen_delim(const char c) {
    return  0x3A == c /*:*/|| 0x2F == c /*rev\*/||
            0x3F == c /*?*/|| 0x23 == c /*#*/||
            0x5B == c /*[*/|| 0x5D == c /*]*/||
            0x40 == c /*@*/;
}

int uri_is_reserved(const char c) {
    return uri_is_gen_delim(c) || uri_is_sub_delim(c);
}

int uri_is_unreserved(const char c) {
    return  abnf_is_ALPHA(c) || abnf_is_DIGIT(c) ||
            0x2D == c /*-*/|| 0x2E == c /*.*/||
            0x5F == c /*_*/|| 0x7E == c /*~*/;
}

int uri_is_pct_encoded(const char* s) {
    return  0x25 == *s /*%*/&&
            abnf_is_HEXDIG(*(s + 1)) &&
            abnf_is_HEXDIG(*(s + 2));
}

int uri_is_pchar(const char* s) {
    return  uri_is_unreserved(*s) ||
            uri_is_pct_encoded(s) ||
            uri_is_sub_delim(*s) ||
            0x3A == *s /*:*/||
            0x40 == *s /*@*/;
}

int uri_is_query(const char* s, int len) {
    int i = 0;
    while (i < len) {
        if (uri_is_pct_encoded(s + i)) {
            i += 3;
        }
        else if (   uri_is_pchar(s + i) ||
                    0x2F == *(s + i) /*rev\*/||
                    0x3F == *(s + i) /*?*/) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_fragment(const char* s, int len) {
    return uri_is_query(s, len);
}

int uri_is_segment(const char* s, int len) {
    int i = 0;
    while (i < len) {
        if (uri_is_pct_encoded(s + i)) {
            if (i + 2 >= len) {
                return 0;
            }
            i += 3;
        }
        else if (uri_is_pchar(s + i)) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_segment_nz(const char* s, int len) {
    return  (len > 0) &&
            uri_is_segment(s, len);
}

int uri_is_segment_nz_nc(const char* s, int len) {
    int i = 0;
    if (0 == len) {
        return 0;
    }
    while (i < len) {
        if (uri_is_pct_encoded(s + i)) {
            if (i + 2 >= len) {
                return 0;
            }
            i += 3;
        }
        else if (   uri_is_unreserved(*(s + i)) ||
                    uri_is_sub_delim(*(s + i)) ||
                    0x40 == *(s + i) /*@*/) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_path_empty(const char* s, int len) {
    return 0 == len;
}

int uri_is_path_rootless(const char* s, int len) {
    int first_sl = 0;
    if (0 == len) {
        return 0;
    }
    while (first_sl < len) {
        if (0x2F == *(s + first_sl) /*rev\*/) {
            break;
        }
        else {
            first_sl += 1;
        }
    }
    if (0 == first_sl) {
        return 0;
    }
    else if (first_sl == len) {
        return uri_is_segment_nz(s, len);
    }
    return  uri_is_segment_nz(s, first_sl) &&
            uri_is_path_abempty(s + first_sl, len - first_sl);
}

int uri_is_path_noscheme(const char* s, int len) {
    int first_sl = 0;
    if (0 == len) {
        return 0;
    }
    while (first_sl < len) {
        if (0x2F == *(s + first_sl) /*rev\*/) {
            break;
        }
        else {
            first_sl += 1;
        }
    }
    if (0 == first_sl) {
        return 0;
    }
    else if (first_sl == len) {
        return uri_is_segment_nz_nc(s, len);
    }
    return  uri_is_segment_nz_nc(s, first_sl) &&
            uri_is_path_abempty(s + first_sl, len - first_sl);
}

int uri_is_path_absolute(const char* s, int len) {
    int second_sl = 1;
    if (0 == len) {
        return 0;
    }
    else if (0x2F == *s /*rev\*/) {
        if (1 == len) {
            return 1;
        }
        while (second_sl < len) {
            if (0x2F == *(s + second_sl) /*rev\*/) {
                break;
            }
            else {
                second_sl += 1;
            }
        }
        if (second_sl == len) {
            return uri_is_segment_nz(s + 1, len - 1);
        }
        else {
            return  uri_is_segment_nz(s + 1, second_sl - 1) &&
                    uri_is_path_abempty(s + second_sl, len - second_sl);
        }
    }
    return 0;
}

int uri_is_path_abempty(const char* s, int len) {
    int second_sl = 1;
    if (0 == len) {
        return 1;
    }
    else if (0x2F == *s /*rev\*/) {
        while (second_sl < len) {
            if (0x2F == *(s + second_sl) /*rev\*/) {
                break;
            }
            else {
                second_sl += 1;
            }
        }
        if (second_sl == len) {
            return uri_is_segment(s + 1, len - 1);
        }
        else {
            return  uri_is_segment(s + 1, second_sl - 1) &&
                    uri_is_path_abempty(s + second_sl, len - second_sl);
        }
    }
    return 0;
}

int uri_is_path(const char* s, int len) {
    return  uri_is_path_empty(s, len) ||
            uri_is_path_absolute(s, len) ||
            uri_is_path_abempty(s, len) ||
            uri_is_path_noscheme(s, len) ||
            uri_is_path_rootless(s, len);
}

int uri_is_reg_name(const char* s, int len) {
    int i = 0;
    while (i < len) {
        if (uri_is_pct_encoded(s + i)) {
            if (i + 2 >= len) {
                return 0;
            }
            i += 3;
        }
        else if (   uri_is_unreserved(*(s + i)) ||
                    uri_is_sub_delim(*(s + i))) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_dec_octet(const char* s, int len) {
    switch (len) {
        case 1:
            return abnf_is_DIGIT(*s);/*0-9*/
        case 2:
            return  0x31 <= *s && *s <= 0x39 &&
                    abnf_is_DIGIT(*(s + 1));/*1-9 0-9*/
        case 3:
            switch (*s) {
                case 0x31:
                    return  abnf_is_DIGIT(*(s + 1)) &&
                            abnf_is_DIGIT(*(s + 2)); /*1 0-9 0-9*/
                case 0x32:
                    switch (*(s + 1)) {
                        case 0x30:
                        case 0x31:
                        case 0x32:
                        case 0x33:
                        case 0x34:
                            return abnf_is_DIGIT(*(s + 2)); /*2 0-4 0-9*/
                        case 0x35:
                            return 0x30 <= *(s + 2) && *(s + 2) <= 0x35; /*2 5 0-5*/
                        default:
                            return 0;
                    }
                default:
                    return 0;
            }
        default:
            return 0;
    }
    return 0;
}

int uri_is_IPv4address(const char* s, int len) {
    /*one past the index of previous "." or 0 in the beginning*/
    int past_prev_dot_index = 0;
    /*past_prev_dot_index + 1 i.e., we start looking for next "." from here*/
    int next_dot_index = 1;
    int i = 0;
    /*(dec-octet "." dec-octet "." dec-octet "." dec-octet) has 7-15 characters*/
    if (len < 7 || len > 15) {
        return 0;
    }
    for (i = 0; i < 3; ++i) { /*find and check first three 1*3DIGIT "."*/
        while (next_dot_index < len) {
            if (0x2E == *(s + next_dot_index) /*.*/) {
                break;
            }
            else {
                next_dot_index += 1;
            }
        }
        if (len == next_dot_index) {
            return 0;
        }
        else if (uri_is_dec_octet(s + past_prev_dot_index,
                    next_dot_index - past_prev_dot_index)) {
            past_prev_dot_index = next_dot_index + 1;
            next_dot_index = past_prev_dot_index + 1;
            continue;
        }
        else {
            return 0;
        }
    }
    /*the rest must be dec-octet*/
    return uri_is_dec_octet(s + past_prev_dot_index, len - past_prev_dot_index);
}

int uri_is_ls32(const char* s, int len) {
    int colon_index = 1;
    while (colon_index < len) {
        if (0x3A == *(s + colon_index) /*:*/) {
            break;
        }
        colon_index += 1;
    }
    if (colon_index >= len) { /*there is no ":", it has to be IPv4address*/
        return uri_is_IPv4address(s, len);
    }
    return  uri_is_h16(s, colon_index) &&
            uri_is_h16(s + colon_index + 1, len - (colon_index + 1));
}

int uri_is_h16(const char* s, int len) {
    /*1*4HEXDIG*/
    int i = 0;
    if (len < 1 || len > 4) {
        return 0;
    }
    while (i < len) {
        if (!abnf_is_HEXDIG(*(s + i))) {
            return 0;
        }
        i += 1;
    }
    return 1;
}

int uri_is_IPv6address(const char* s, int len) {
    int past_prev_colon = 0;
    int next_colon = -1;
    int h16_count = 0;
    int has_double_colon = 0;
    int i = 0;
    if (len < 2) {
        return 0;
    }
    /*if it starts with ::*/
    if (0x3A == *s && 0x3A == *(s + 1) /*::*/) {
        has_double_colon = 1;
        past_prev_colon = 2;
        i = 2;
    }
    while (i < len) {
        /*next : found*/
        if (0x3A == *(s + i) /*:*/) {
            next_colon = i;
            if (uri_is_h16(s + past_prev_colon,
                        next_colon - past_prev_colon)) {
                h16_count += 1;
                if (    next_colon + 1 < len &&
                        0x3A == *(s + next_colon + 1) /*:*/) {
                    if (has_double_colon) {
                        return 0; /*only one :: allowed*/
                    }
                    has_double_colon = 1;
                    past_prev_colon = next_colon + 2;
                    i += 2;
                    continue;
                }
                past_prev_colon = next_colon + 1;
                i += 1;
                continue;
            }
            return 0; /*it wasn't h16*/
        }
        /*first . found*/
        if (0x2E == *(s + i) /*.*/) {
            if (uri_is_IPv4address(s + past_prev_colon, len - past_prev_colon)) {
                h16_count += 2;
                break;
            }
            else {
                return 0;
            }
        }
        if (!abnf_is_HEXDIG(*(s + i))) {
            return 0;
        }
        else {
            i += 1;
        }
    }
    /*i < len, so we exited loop by break, so there is IPv4 tail*/
    if (i < len) {
        if (has_double_colon) {
            return h16_count <= 8;
        }
        return h16_count == 8;
    }
    /*ther should be additional trailing h16 or the entire string is "::"*/
    else {
        if (has_double_colon && (2 == len)) {
            return 1;
        }
        else if (uri_is_h16(s + past_prev_colon, len - past_prev_colon)) {
            h16_count += 1;
        }
        else {
            return 0;
        }
        if (has_double_colon) {
            return h16_count <= 8;
        }
        return h16_count == 8;
    }
    return 0; /*it should never happen*/
}

int uri_is_IPvFuture(const char* s, int len) {
    int i = 0;
    if (len < 4) {
        return 0;
    }
    if (!abnf_is_V(*(s + i))) { /*should start with "v"*/
        return 0;
    }
    i += 1;
    while (i < len && abnf_is_HEXDIG(*(s + i))) {
        i += 1;
    }
    if ((1 == i) || (len == i)) { /*should have at leas one HEXDIG*/
        return 0;
    }
    if (0x2E != *(s + i) /*.*/) { /*should be .*/
        return 0;
    }
    i += 1;
    if (i == len) { /*should have at leas one more character*/
        return 0;
    }
    while (i < len) { /*from now till end ( unreserved / sub-delims / ":" )*/
        if (    uri_is_unreserved(*(s + i)) ||
                uri_is_sub_delim(*(s + i)) ||
                0x3A == *(s + i) /*:*/) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_IP_literal(const char* s, int len) {
    if (    (len < 3) ||
            0x5B != *s /*[*/||
            0x5D != *(s + len - 1)/*]*/) {
        return 0;
    }
    return  uri_is_IPvFuture(s + 1, len - 2) ||
            uri_is_IPv6address(s + 1, len - 2);
}

int uri_is_port(const char* s, int len) {
    int i = 0;
    while (i < len) {
        if (!abnf_is_DIGIT(*(s + i))) {
            return 0;
        }
        i += 1;
    }
    return 1;
}

int uri_is_host(const char* s, int len) {
    return  uri_is_IP_literal(s, len) ||
            uri_is_IPv4address(s, len) ||
            uri_is_reg_name(s, len);
}

int uri_is_userinfo(const char* s, int len) {
    int i = 0;
    while (i < len) {
        if (uri_is_pct_encoded(s + i)) {
            if (i + 2 >= len) {
                return 0;
            }
            i += 3;
        }
        else if (   uri_is_unreserved(*(s + i)) ||
                    uri_is_sub_delim(*(s + i)) ||
                    0x3A == *(s + i) /*:*/) {
            i += 1;
        }
        else {
            return 0;
        }
    }
    return 1;
}

int uri_is_authority(const char* s, int len) {
    /*authority = [ userinfo "@" ] host [ ":" port ]*/
    int last_colon_index = len - 1;
    int first_at_index = 0;
    while (last_colon_index >= 0) {
        if (0x3A == *(s + last_colon_index) /*:*/) {
            break;
        }
        last_colon_index -= 1;
    }
    while (first_at_index < len) {
        if (0x40 == *(s + first_at_index) /*@*/) {
            break;
        }
        first_at_index += 1;
    }
    if (first_at_index == len) { /*there is no userinfo part*/
        first_at_index = -1;
    }
    if (last_colon_index < first_at_index) { /*there is no port part*/
        last_colon_index = -1;
    }
    if (first_at_index == -1 && last_colon_index == -1) {
        return uri_is_host(s, len);
    }
    else if (last_colon_index == -1) {
        return  uri_is_userinfo(s, first_at_index) &&
                uri_is_host(s + first_at_index + 1, len - (first_at_index + 1));
    }
    /*  if there is : after @ it is either part of host (and there is no port)
     *  or it separates host from port*/
    else if (first_at_index == -1) { 
        return  (uri_is_host(s, last_colon_index) &&
                    uri_is_port(s + last_colon_index + 1,
                        len - (last_colon_index + 1))) ||
                uri_is_host(s, len);
    }
    else {
        return  uri_is_userinfo(s, first_at_index) &&
                ((uri_is_host(s + first_at_index + 1,
                        last_colon_index - (first_at_index + 1)) &&
                  uri_is_port(s + last_colon_index + 1,
                        len - (last_colon_index + 1))) ||
                 uri_is_host(s + first_at_index + 1,
                        len - (first_at_index + 1)));
    }
    return 0; /*it should never happen*/
}

int uri_is_scheme(const char* s, int len) {
    int i = 1;
    if (len < 1) {
        return 0;
    }
    if (!abnf_is_ALPHA(*s)) {
        return 0;
    }
    while (i < len) {
        if (!(abnf_is_ALPHA(*(s + i)) ||
              abnf_is_DIGIT(*(s + i)) ||
              0x2B == *(s + i) /*+*/||
              0x2D == *(s + i) /*-*/||
              0x2E == *(s + i) /*.*/)) {
            return 0;
        }
        i += 1;
    }
    return 1;
}

int uri_is_relative_part(const char* s, int len) {
    int last_sl = len - 1;
    /*"rev\rev\" authority path-abempty*/
    if (len >= 2 && 0x2F == *s && 0x2F == *(s + 1) /*rev\rev\*/) {
        while (last_sl > 1) {
            if (0x2F == *(s + last_sl) /*rev\*/) {
                break;
            }
            last_sl -= 1;
        }
        if (last_sl <= 1) { /*there is no path-abempty part*/
            return uri_is_authority(s + 2, len - 2);
        }
        return  uri_is_authority(s + 2, last_sl - 2) &&
                uri_is_path_abempty(s + last_sl, len - last_sl);
    }
    return  uri_is_path_empty(s, len) ||
            uri_is_path_absolute(s, len) ||
            uri_is_path_noscheme(s, len);
}

int uri_is_relative_ref(const char* s, int len) {
    int query_start = -1;
    int fragment_start = -1;
    int i = 0;
    while (i < len) {
        if (query_start == -1 && 0x3F == *(s + i) /*?*/) {
            query_start = i;
        }
        if (fragment_start == -1 && 0x23 == *(s + i) /*#*/) {
            fragment_start = i;
        }
        i += 1;
    }
    /*if first ? is after first # there is no query it is a part of fragment*/
    if (fragment_start != -1 && query_start > fragment_start) {
        query_start = -1;
    }
    if (query_start == -1 && fragment_start == -1) { /*no query or fragment*/
        return uri_is_relative_part(s, len);
    }
    else if (query_start == -1) { /*no query*/
        return  uri_is_relative_part(s, fragment_start) &&
                uri_is_fragment(s + fragment_start + 1, len - (fragment_start + 1));
    }
    else if (fragment_start == -1) { /*no fragment*/
        return  uri_is_relative_part(s, query_start) &&
                uri_is_query(s + query_start + 1, len - (query_start + 1));
    }
    else { /*both query and fragment*/
        return  uri_is_relative_part(s, query_start) &&
                uri_is_query(s + query_start + 1, fragment_start - (query_start + 1)) &&
                uri_is_fragment(s + fragment_start + 1, len - (fragment_start + 1));
    }
    return 0; /*it should never happen*/
}

int uri_is_absolute_uri(const char* s, int len) {
    /*absolute-URI = scheme ":" hier-part [ "?" query ]*/
    int i = 0;
    int hier_part_start = -1;
    while (i < len) {
        if (0x3A == *(s + i) /*:*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*There is no :*/
        return 0;
    }
    if (!uri_is_scheme(s, i)) { /*scheme should be befre first :*/
        return 0;
    }
    i += 1;
    hier_part_start = i;
    while (i < len) {
        if (0x3F == *(s + i) /*?*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*There is no ( "?" query )*/
        return uri_is_hier_part(s + hier_part_start, len - hier_part_start);
    }
    /*There are both hier-part and ( "?" query )*/
    return  uri_is_hier_part(s + hier_part_start, i - hier_part_start) &&
            uri_is_query(s + i + 1, len - (i + 1));
}

int uri_is_uri_reference(const char* s, int len) {
    return  uri_is_uri(s, len) ||
            uri_is_relative_ref(s, len);
}

int uri_is_hier_part(const char* s, int len) {
    int last_sl = len - 1;
    /*"rev\rev\" authority path-abempty*/
    if (len >= 2 && 0x2F == *s && 0x2F == *(s + 1) /*rev\rev\*/) {
        while (last_sl > 1) {
            if (0x2F == *(s + last_sl) /*rev\*/) {
                break;
            }
            last_sl -= 1;
        }
        if (last_sl <= 1) { /*there is no path-abempty part*/
            return uri_is_authority(s + 2, len - 2);
        }
        return  uri_is_authority(s + 2, last_sl - 2) &&
                uri_is_path_abempty(s + last_sl, len - last_sl);
    }
    return  uri_is_path_empty(s, len) ||
            uri_is_path_absolute(s, len) ||
            uri_is_path_rootless(s, len);
}

int uri_is_uri(const char* s, int len) {
    /*URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]*/
    int i = 0;
    int hier_part_start = -1;
    int query_start = -1;
    while (i < len) {
        if (0x3A == *(s + i) /*:*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*There is no :*/
        return 0;
    }
    if (!uri_is_scheme(s, i)) { /*scheme should be befre first :*/
        return 0;
    }
    i += 1;
    hier_part_start = i;
    while (i < len) {
        if (    0x3F == *(s + i) /*?*/||
                0x23 == *(s + i) /*#*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*There is no ( "?" query ) or ( "#" fragment )*/
        return uri_is_hier_part(s + hier_part_start, len - hier_part_start);
    }
    if (0x3F == *(s + i) /*?*/) {
        if (!uri_is_hier_part(s + hier_part_start, i - hier_part_start)) {
            return 0;
        }
        i += 1;
        query_start = i; /*if we've found ? before # there should be query*/
    }
    while (i < len) {
        if (0x23 == *(s + i) /*#*/) {
            break;
        }
        i += 1;
    }
    if (i == len) { /*there is no fragment*/
        return uri_is_query(s + query_start, len - query_start);
    }
    /*there should be query "#" fragment*/
    return  uri_is_query(s + query_start, i - query_start) &&
            uri_is_fragment(s + i + 1, len - (i + 1));
}

