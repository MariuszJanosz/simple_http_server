#ifndef ABNF_H
#define ABNF_H

#include <stddef.h>

/*Letters*/
int abnf_is_A(const unsigned char c);
int abnf_is_B(const unsigned char c);
int abnf_is_C(const unsigned char c);
int abnf_is_D(const unsigned char c);
int abnf_is_E(const unsigned char c);
int abnf_is_F(const unsigned char c);
int abnf_is_G(const unsigned char c);
int abnf_is_H(const unsigned char c);
int abnf_is_I(const unsigned char c);
int abnf_is_J(const unsigned char c);
int abnf_is_K(const unsigned char c);
int abnf_is_L(const unsigned char c);
int abnf_is_M(const unsigned char c);
int abnf_is_N(const unsigned char c);
int abnf_is_O(const unsigned char c);
int abnf_is_P(const unsigned char c);
int abnf_is_Q(const unsigned char c);
int abnf_is_R(const unsigned char c);
int abnf_is_S(const unsigned char c);
int abnf_is_T(const unsigned char c);
int abnf_is_U(const unsigned char c);
int abnf_is_V(const unsigned char c);
int abnf_is_W(const unsigned char c);
int abnf_is_X(const unsigned char c);
int abnf_is_Y(const unsigned char c);
int abnf_is_Z(const unsigned char c);

/*Digits*/
int abnf_is_0(const unsigned char c);
int abnf_is_1(const unsigned char c);
int abnf_is_2(const unsigned char c);
int abnf_is_3(const unsigned char c);
int abnf_is_4(const unsigned char c);
int abnf_is_5(const unsigned char c);
int abnf_is_6(const unsigned char c);
int abnf_is_7(const unsigned char c);
int abnf_is_8(const unsigned char c);
int abnf_is_9(const unsigned char c);

/*ABNF core rules*/
int abnf_is_ALPHA(const unsigned char c);
int abnf_is_BIT(const unsigned char c);
int abnf_is_CHAR(const unsigned char c);
int abnf_is_CR(const unsigned char c);
int abnf_is_CTL(const unsigned char c);
int abnf_is_DIGIT(const unsigned char c);
int abnf_is_DQUOTE(const unsigned char c);
int abnf_is_HEXDIG(const unsigned char c);
int abnf_is_HTAB(const unsigned char c);
int abnf_is_LF(const unsigned char c);
int abnf_is_OCTET(const unsigned char c);
int abnf_is_SP(const unsigned char c);
int abnf_is_VCHAR(const unsigned char c);
int abnf_is_WSP(const unsigned char c);

int abnf_is_CRLF(const unsigned char* s);

int abnf_is_LWSP(const unsigned char* s, size_t len);

#endif /*ABNF_H*/

