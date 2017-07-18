/*
 * bbierext.h
 *
 *  Created on: 2017Äê6ÔÂ11ÈÕ
 *      Author: Administrator
 */

/*add by Sandy Zhang for Babel BIER extension*/

#ifndef BBIEREXT_H_

#define BBIER_BASE 1
#define BBIER_MPLS 2
#define BBIER_BSL 3

#define BBIER_ROUTE_NEW 1
#define BBIER_ROUTE_UPDATE 2
#define BBIER_ROUTE_ERROR -1

#define BBIER_IPV4_BFRPREFIX 1
#define BBIER_IPV6_BFRPREFIX 2

#define BBIER_GEN_ONE_LENGTH 1
#define BBIER_GEN_TWO_LENGTH 2

/*SUBSUBTLV type must different from BIER-INFO-SUBTLV*/
#define BABEL_BIER_MPLS_SUBSUBTLV 1
#define BABEL_BIER_BSL_SUBSUBTLV  2

struct bbier_info_subtlv_format
{
    unsigned char type;
    unsigned char length;
    unsigned char rev;
    unsigned char sid;
    unsigned short bid;
};

struct bbier_mpls_subtlv_format
{
    unsigned char type;
    unsigned char length;

	unsigned char lbl_range;
    unsigned char bsl;  /*4 bits*/
    unsigned int label;   /*20bits*/
};

struct bbier_bsl_subtlv_format
{
    unsigned char type;
    unsigned char bsl_length;
};

struct bbier_route_mpls {
	unsigned char label_range_size;
	unsigned char bsl;
	unsigned int label_value;
	unsigned int mix_value;
	struct bbier_route_mpls *bbrsm_next;
};

struct bbier_route_sub {
    unsigned char subdomain_id;
    unsigned char reserved;
    unsigned short bfr_id;

    unsigned short bsl_conversion;

    unsigned short bbmpls_count;
    struct bbier_route_mpls *bb_mpls;

    struct bbier_route_sub *bbrs_next;
};

struct bbier_route {
    struct interface *ifp; /*For local configuration, it is empty*/
    unsigned char local_flag; /*For local configuration, it is set to 1*/
    unsigned char ae_flag;
    unsigned short last_upd_seqno; /*Used to test if it is the newest*/
    int change_flag;

    unsigned char ipv4_prefix[4];
    unsigned char ipv6_prefix[16];

    unsigned short ipv4_sdcount; /*number of sub-domain id*/
    unsigned short ipv6_sdcount; /*number of sub-domain id*/

    struct bbier_route_sub *ipv4_bbrs;
    struct bbier_route_sub *ipv6_bbrs;

    struct bbier_route *bbier_next;
};

void bbier_initial_sub(int ae, unsigned char *prefix, unsigned char sid, unsigned short bid);
void bbier_initial(void);
void bbier_insert_route(struct bbier_route *bbier);
void bbier_del_route(unsigned char *prefix, int ae);
int bbier_insert_bbier_info(struct bbier_route *bbier, struct bbier_route_sub *bbrs_new, int ae);
struct bbier_route_mpls *bbier_locate_bbrsm(struct bbier_route_sub*bbrs, unsigned char bsl);
int bbier_insert_bbrsm(struct bbier_route_sub *bbrs, struct bbier_route_mpls *bbrsm_new);
struct bbier_route_sub *bbier_search_subdomain_id(struct bbier_route *bbier, unsigned char subid, int ae);
struct bbier_route *bbier_insert_prefix(int ae, unsigned char *src_prefix, unsigned short sid, unsigned short bid, int local_flag);
struct bbier_route *bbier_check_prefix_local(int ae, unsigned char *src_prefix, unsigned char *flag);
int parse_bbier_info_subtlv(const unsigned char *pp, int ae, unsigned char *src_prefix);
void bbier_release_bbrsm(struct bbier_route_sub *bbrs, struct bbier_route_mpls *bbrsm);
void bbier_delallin_bbrs(int ae, struct bbier_route_sub *bbrs);
void bbier_release_bbrs(struct bbier_route *bbier, struct bbier_route_sub *bbrs);
void bbier_delallin_bbier(struct bbier_route *bbier);
void bbier_release_bbier(struct bbier_route *bbier);
void bbier_notify_route(struct babel_route *route);
struct bbier_route *bbier_locate_prefix(int ae, const unsigned char *src_prefix);
void bbier_del_bbier_route(struct babel_route *route);
int bbier_call_fillin(unsigned char *buf, const unsigned char *prefix, unsigned char len);
int bbier_fillin_info_subtlv(unsigned char *pp, int ae, const unsigned char *prefix/*, int *length*/);


#define BBIEREXT_H_



#endif /* BBIEREXT_H_ */
