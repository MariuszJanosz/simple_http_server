#include "abnf.h"

#include <stddef.h>

/*Letters*/
int abnf_is_A(const unsigned char c) {
    return 0x41 == c || 0x61 == c;/*A||a*/
}

int abnf_is_B(const unsigned char c) {
    return 0x42 == c || 0x62 == c;/*B||b*/
}

int abnf_is_C(const unsigned char c) {
    return 0x43 == c || 0x63 == c;/*C||c*/
}

int abnf_is_D(const unsigned char c) {
    return 0x44 == c || 0x64 == c;/*D||d*/
}

int abnf_is_E(const unsigned char c) {
    return 0x45 == c || 0x65 == c;/*E||e*/
}

int abnf_is_F(const unsigned char c) {
    return 0x46 == c || 0x66 == c;/*F||f*/
}

int abnf_is_G(const unsigned char c) {
    return 0x47 == c || 0x67 == c;/*G||g*/
}

int abnf_is_H(const unsigned char c) {
    return 0x48 == c || 0x68 == c;/*H||h*/
}

int abnf_is_I(const unsigned char c) {
    return 0x49 == c || 0x69 == c;/*I||i*/
}

int abnf_is_J(const unsigned char c) {
    return 0x4A == c || 0x6A == c;/*J||j*/
}

int abnf_is_K(const unsigned char c) {
    return 0x4B == c || 0x6B == c;/*K||k*/
}

int abnf_is_L(const unsigned char c) {
    return 0x4C == c || 0x6C == c;/*L||l*/
}

int abnf_is_M(const unsigned char c) {
    return 0x4D == c || 0x6D == c;/*M||m*/
}

int abnf_is_N(const unsigned char c) {
    return 0x4E == c || 0x6E == c;/*N||n*/
}

int abnf_is_O(const unsigned char c) {
    return 0x4F == c || 0x6F == c;/*O||o*/
}

int abnf_is_P(const unsigned char c) {
    return 0x50 == c || 0x70 == c;/*P||p*/
}

int abnf_is_Q(const unsigned char c) {
    return 0x51 == c || 0x71 == c;/*Q||q*/
}

int abnf_is_R(const unsigned char c) {
    return 0x52 == c || 0x72 == c;/*R||r*/
}

int abnf_is_S(const unsigned char c) {
    return 0x53 == c || 0x73 == c;/*S||s*/
}

int abnf_is_T(const unsigned char c) {
    return 0x54 == c || 0x74 == c;/*T||t*/
}

int abnf_is_U(const unsigned char c) {
    return 0x55 == c || 0x75 == c;/*U||u*/
}

int abnf_is_V(const unsigned char c) {
    return 0x56 == c || 0x76 == c;/*V||v*/
}

int abnf_is_W(const unsigned char c) {
    return 0x57 == c || 0x77 == c;/*W||w*/
}

int abnf_is_X(const unsigned char c) {
    return 0x58 == c || 0x78 == c;/*X||x*/
}

int abnf_is_Y(const unsigned char c) {
    return 0x59 == c || 0x79 == c;/*Y||y*/
}

int abnf_is_Z(const unsigned char c) {
    return 0x5A == c || 0x7A == c;/*Z||z*/
}

/*Digits*/
int abnf_is_0(const unsigned char c) {
    return 0x30 == c;/*0*/
}

int abnf_is_1(const unsigned char c) {
    return 0x31 == c;/*1*/
}

int abnf_is_2(const unsigned char c) {
    return 0x32 == c;/*2*/
}

int abnf_is_3(const unsigned char c) {
    return 0x33 == c;/*3*/
}

int abnf_is_4(const unsigned char c) {
    return 0x34 == c;/*4*/
}

int abnf_is_5(const unsigned char c) {
    return 0x35 == c;/*5*/
}

int abnf_is_6(const unsigned char c) {
    return 0x36 == c;/*6*/
}

int abnf_is_7(const unsigned char c) {
    return 0x37 == c;/*7*/
}

int abnf_is_8(const unsigned char c) {
    return 0x38 == c;/*8*/
}

int abnf_is_9(const unsigned char c) {
    return 0x39 == c;/*9*/
}

/*ABNF core rules*/
int abnf_is_ALPHA(const unsigned char c) {
                    /*A-Z                         a-z*/
    return (0x41 <= c && c <= 0x5A) || (0x61 <= c && c <= 0x7A);
}

int abnf_is_BIT(const unsigned char c) {
    return abnf_is_0(c) || abnf_is_1(c);
}

int abnf_is_CHAR(const unsigned char c) {
    return 0x01 <= c && c <= 0x7F; /*Any 7-bit ASCII except NUL*/
}

int abnf_is_CR(const unsigned char c) {
    return c == 0x0D; /*CR*/
}

int abnf_is_CTL(const unsigned char c) {
                    /*NUL-US              DEL*/
    return (0x00 <= c && c <= 0x1F) || (c == 0x7F);
}

int abnf_is_DIGIT(const unsigned char c) {
    return 0x30 <= c && c <= 0x39; /*0-9*/
}

int abnf_is_DQUOTE(const unsigned char c) {
    return 0x22 == c; /*"*/
}

int abnf_is_HEXDIG(const unsigned char c) {
    return  abnf_is_DIGIT(c) || abnf_is_A(c) || abnf_is_B(c) || abnf_is_C(c) || abnf_is_D(c) ||
            abnf_is_E(c) || abnf_is_F(c);
}

int abnf_is_HTAB(const unsigned char c) {
    return 0x09 == c;
}

int abnf_is_LF(const unsigned char c) {
    return 0x0A == c;
}

int abnf_is_OCTET(const unsigned char c) {
    return 0x00 <= c && c <= 0xFF;
}

int abnf_is_SP(const unsigned char c) {
    return 0x20 == c;
}

int abnf_is_VCHAR(const unsigned char c) {
    return 0x21 <= c && c <= 0x7E; /*Visible characters, no control or space*/
}

int abnf_is_WSP(const unsigned char c) {
    return abnf_is_SP(c) || abnf_is_HTAB(c);
}

int abnf_is_CRLF(const unsigned char* s) {
    if (abnf_is_CR(*s)) return abnf_is_LF(*(s + 1));
    return 0;
}

/* LWSP = *(WSP / CRLF WSP) */
int abnf_is_LWSP(const unsigned char* s, size_t len) {
    if (len == 0) return 1;
    int i = 0;
    while (i < len) {
        if (abnf_is_WSP(*(s + i))) {
            i += 1;
        }
        else if (abnf_is_CRLF(s + i)) {
            if (i + 2 >= len) {
                return 0;
            }
            i += 2;
        }
        else {
            return 0;
        }
    }
    return 1;
}

