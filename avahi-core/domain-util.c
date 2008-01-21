/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <stdio.h>

#include <avahi-common/malloc.h>
#include <avahi-common/defs.h>
#include <avahi-core/dns.h>

#include "log.h"
#include "domain-util.h"
#include "util.h"

static void strip_bad_chars(char *s) {
    char *p, *d;

    s[strcspn(s, ".")] = 0;
    
    for (p = s, d = s; *p; p++) 
        if ((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '-')
            *(d++) = *p;

    *d = 0;
}

#ifdef __linux__
static int load_lsb_distrib_id(char *ret_s, size_t size) {
    FILE *f;
    
    assert(ret_s);
    assert(size > 0);

    if (!(f = fopen("/etc/lsb-release", "r")))
        return -1;

    while (!feof(f)) {
        char ln[256], *p;

        if (!fgets(ln, sizeof(ln), f))
            break;

        if (strncmp(ln, "DISTRIB_ID=", 11))
            continue;

        p = ln + 11;
        p += strspn(p, "\"");
        p[strcspn(p, "\"")] = 0;

        snprintf(ret_s, size, "%s", p);

        fclose(f);
        return 0;
    }

    fclose(f);
    return -1;
}
#endif

char *avahi_get_host_name(char *ret_s, size_t size) {
    assert(ret_s);
    assert(size > 0);

    if (gethostname(ret_s, size) >= 0) {
        ret_s[size-1] = 0;
        strip_bad_chars(ret_s);
    } else
        *ret_s = 0;

    if (strcmp(ret_s, "localhost") == 0 || strncmp(ret_s, "localhost.", 10) == 0) {
        *ret_s = 0;
        avahi_log_warn("System host name is set to 'localhost'. This is not a suitable mDNS host name, looking for alternatives.");
    }
    
    if (*ret_s == 0) {
        /* No hostname was set, so let's take the OS name */

#ifdef __linux__

        /* Try LSB distribution name first */
        if (load_lsb_distrib_id(ret_s, size) >= 0) {
            strip_bad_chars(ret_s);
            avahi_strdown(ret_s);
        }

        if (*ret_s == 0)
#endif

        {
            /* Try uname() second */
            struct utsname utsname;
            
            if (uname(&utsname) >= 0) {
                snprintf(ret_s, size, "%s", utsname.sysname);
                strip_bad_chars(ret_s);
                avahi_strdown(ret_s);
            }

            /* Give up */
            if (*ret_s == 0)
                snprintf(ret_s, size, "unnamed");
        }
    }

    if (size >= AVAHI_LABEL_MAX)
	ret_s[AVAHI_LABEL_MAX-1] = 0;    
    
    return ret_s;
}

char *avahi_get_host_name_strdup(void) {
    char t[AVAHI_DOMAIN_NAME_MAX];

    if (!(avahi_get_host_name(t, sizeof(t))))
        return NULL;

    return avahi_strdup(t);
}

int avahi_binary_domain_cmp(const char *a, const char *b) {
    assert(a);
    assert(b);

    if (a == b)
        return 0;

    for (;;) {
        char ca[AVAHI_LABEL_MAX], cb[AVAHI_LABEL_MAX], *p;
        int r;

        p = avahi_unescape_label(&a, ca, sizeof(ca));
        assert(p);
        p = avahi_unescape_label(&b, cb, sizeof(cb));
        assert(p);

        if ((r = strcmp(ca, cb)))
            return r;
        
        if (!*a && !*b)
            return 0;
    }
}

int avahi_domain_ends_with(const char *domain, const char *suffix) {
    assert(domain);
    assert(suffix);

    for (;;) {
        char dummy[AVAHI_LABEL_MAX], *r;

        if (*domain == 0)
            return 0;
        
        if (avahi_domain_equal(domain, suffix))
            return 1;

        r = avahi_unescape_label(&domain, dummy, sizeof(dummy));
        assert(r);
    } 
}

/*todo: revise location of this function in this file vs domain.c (and.h) */
unsigned char * avahi_c_to_canonical_string(const char* input)
    {
        char *label = avahi_malloc(AVAHI_LABEL_MAX);
        char *retval = avahi_malloc(AVAHI_DOMAIN_NAME_MAX);
        unsigned char *result = retval;

        /* printf("invoked with: -%s-\n", input); */

        for(;;)
            {
             avahi_unescape_label(&input, label, AVAHI_LABEL_MAX);

             if(!(*label))
                break;

             *result = (char)strlen(label);
             /* printf("label length: -%d-\n", *result); */

             result++;

             /*printf("label: -%s-\n", label); */

             strcpy(result, label);
             result += (char)strlen(label);

             /* printf("intermediate result: -%s-\n", retval); */
            }

       /* printf("result: -%s-\n", retval);
          printf("result length: -%d-\n", (char)strlen(retval)); */

       avahi_free(label);
       return retval;
    }

unsigned char * avahi_uint16_to_canonical_string(uint16_t v) {
    uint8_t *c = avahi_malloc(2);

    c[0] = (uint8_t) (v >> 8);
    c[1] = (uint8_t) v;
    return c;
}

unsigned char * avahi_uint32_to_canonical_string(uint32_t v) {
    uint8_t *c = avahi_malloc(4);

    c[0] = (uint8_t) (v >> 24);
    c[1] = (uint8_t) (v >> 16);
    c[2] = (uint8_t) (v >> 8);
    c[3] = (uint8_t) v;

    return c;
}

uint8_t avahi_count_canonical_labels(const char* input){
    char *p;
    uint8_t count;

    p = input;
    count = 0;

    while (*p != 0){
          count++;
          p += *p;
    }

    return count;
}

/* reference keytag generator from RFC 4034 */
/* invoke with keytag(<rdata>, <rdlength>); */
uint16_t keytag(uint8_t key[], uint16_t keysize){
    uint32_t ac;
    int i;

    for (ac = 0, i = 0; i < keysize; ++i)
        ac += (i & 1) ? key[i] : key[i] << 8;

    ac += (ac >> 16) & 0xFFFF;

    return ac & 0xFFFF;
   }

/*invoke with avahi_keytag(<RR>); */
uint16_t avahi_keytag(AvahiRecord *r){
    uint16_t result;
    AvahiDnsPacket *tmp;

    if (r->key->type != AVAHI_DNS_TYPE_RRSIG)
        return NULL; /* invalid RRTYPE to generate keytag on */

    tmp = avahi_dns_packet_new_query(0); /* MTU */

    if (!tmp) { /*OOM check */
      avahi_log_error("avahi_dns_packet_new_query() failed.");
      assert(tmp);
    }

    /* no TTL binding, leave record unaltered */
    result = avahi_dns_packet_append_record(tmp, r, 0, 0);

    if (!result) {
      avahi_log_error("appending of rdata failed.");
      assert(result);
    }

    /* update RRSET we modified */
    avahi_dns_packet_set_field(tmp, AVAHI_DNS_FIELD_ARCOUNT, 1);

    /* finally, generate keytag */
    /* first arg is rdata address, second arg is rdlength */
    result = keytag(AVAHI_DNS_PACKET_DATA(tmp) + AVAHI_DNS_PACKET_HEADER_SIZE, tmp->size - AVAHI_DNS_PACKET_HEADER_SIZE);

    avahi_free(tmp);

    return result;
}
