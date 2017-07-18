/*add by Sandy Zhang for Babel BIER extension*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include "babeld.h"
#include "util.h"
#include "kernel.h"
#include "interface.h"
#include "source.h"
#include "neighbour.h"
#include "route.h"
#include "xroute.h"
#include "message.h"
#include "resend.h"
#include "configuration.h"
#include "local.h"
#include "disambiguation.h"
#include "bbierext.h"
#include "interface.h"

struct bbier_route *bbier_global = NULL;
int bbier_local_configuration = 0;

void bbier_initial_sub(int ae, unsigned char *prefix, unsigned char sid, unsigned short bid)
{
    struct bbier_route *bbier = NULL;
    struct bbier_route_sub *bbrs_new = NULL;\
    int rtn = 0;

    if ((ae != 1) && (ae != 2))
    {
    	debugf("=====================INI BBIER bbier_global: ae ERROR %d\n", ae);
    }

    bbier = malloc(sizeof(struct bbier_route));
    if (!bbier) {
        fprintf(stderr, "INI SUBTLV_BABEL_BIER_INFO error: No enough memory.\n");
        return;
    }
    memset(bbier, 0, sizeof(struct bbier_route));

    if (ae == 1)
    {
        bbier->local_flag = 1;
        bbier->ae_flag = 1;
    	memcpy(bbier->ipv4_prefix, prefix, 4);
    }
    else if (ae == 2)
    {
    	bbier->local_flag = 1;
    	bbier->ae_flag = 2;
    	memcpy(bbier->ipv6_prefix, prefix, 16);
    }

	bbrs_new = malloc(sizeof(struct bbier_route_sub));
	if (!bbrs_new) {
	    fprintf(stderr, "INI SUBTLV_BABEL_BIER_INFO error: No enough memory.\n");
		return;
    }
	memset(bbrs_new, 0, sizeof(struct bbier_route_sub));
	bbrs_new->subdomain_id = sid;
	bbrs_new->bfr_id = bid;

	rtn = bbier_insert_bbier_info(bbier, bbrs_new, ae);
	if (rtn <= 0) {
	    fprintf(stderr, "INI SUBTLV_BABEL_BIER_INFO error: subdomian_id %d duplicated.\n", sid);
	    return;
	}

	bbier_insert_route(bbier);

	return;
}

void bbier_initial_mpls(int ae, unsigned char *prefix, unsigned char sid, unsigned short bid,
		                unsigned char lbl_range, unsigned char bsl, unsigned int label)
{
	unsigned char flag=0;
	struct bbier_route *bbier = NULL;
	struct bbier_route_sub *bbrs = NULL;
	struct bbier_route_mpls *bbrsm = NULL;
	unsigned int tt_prefix = 0;

	tt_prefix = *(unsigned int *)(prefix);
	debugf("***********************INI MPLS bbier_initial_mpls: ae: %d, tt_prefix:0x%x, sid: %d, bid: %d, lrange: %d, bsl: %d, label: %d\n", ae, tt_prefix, sid, bid, lbl_range, bsl, label);

	bbier = NULL;
	bbier = bbier_check_prefix_local(ae, prefix, &flag);
    if (!bbier)
    	return;

	bbrs = NULL;
	bbrs =  bbier_search_subdomain_id(bbier, sid, ae);
	if (!bbrs)
	    return;

	bbrsm =  bbier_locate_bbrsm(bbrs, bsl);
	if (!bbrsm)
	{
	    bbrsm = malloc(sizeof(struct bbier_route_mpls));
	    if (!bbrsm) {
	        fprintf(stderr, "No enough memory: alloc bbier_route_mpls.\n");
	        return;
	    }
	    memset(bbrsm, 0, sizeof(struct bbier_route_mpls));
	    bbrsm->bsl = bsl;
	    bbrsm->label_range_size = lbl_range;
	    bbrsm->label_value = label;

	    bbier_insert_bbrsm(bbrs, bbrsm);
	}
	else
	{
	    bbrsm->label_range_size = lbl_range;
	    bbrsm->label_value = label;
	}

	return;
}

void bbier_initial(void)
{
	struct interface *ifp = NULL;
	unsigned short temp_bid = 0;
	unsigned int temp_prefix = 0;

	/*return;*/

	if (bbier_local_configuration == 1)
		return;

	temp_bid = rand();

	debugf("=====================INI BBIER Info sub-TLV, subdomain_id: %d, bfr-id: %d\n",
		        0, temp_bid);

	FOR_ALL_INTERFACES(ifp)
	{
	    if (ifp->ipv4)
	    {
	    	temp_prefix = *(unsigned int *)(ifp->ipv4);
	    	debugf("=====================INI BBIER Info sub-TLV, prefix: 0x%x\n", temp_prefix);
	    	bbier_initial_sub(1, ifp->ipv4, 0, temp_bid); /*simplest test*/
	    	temp_prefix = *(unsigned int *)(ifp->ipv4);
	    	debugf("=====================INI BBIER Info mpls sub-sub-TLV, prefix: 0x%x\n", temp_prefix);
	    	bbier_initial_mpls(1, ifp->ipv4, 0, temp_bid, 10, 2, temp_bid);
	    	bbier_local_configuration = 1;
	    }
	    else if (ifp->ll)
	    {
	    	debugf("=====================INI BBIER Info sub-TLV, IPv6\n");
	    	/*bbier_initial_sub(1, ifp->ll, 0, temp_bid);
	    	bbier_initial_mpls(1, ifp->ll, 0, 5, 10, 2, 901);*/
	    	bbier_local_configuration = 1;
	    }
	}
}

void bbier_insert_route(struct bbier_route *bbier)
{
    struct bbier_route *bbier_tmp=NULL, *last_bbier = NULL;

    if (bbier_global == NULL)
    {
    	bbier_global = bbier;
    	return;
    }
    else if (bbier_global == bbier)
    {
    	return;
    }

	bbier_tmp = bbier_global;
	while (bbier_tmp)
	{
		last_bbier = bbier_tmp;

		if ( bbier_tmp == bbier )
		{
			debugf("=====================bbier_insert_route same!\n");
			return;
		}

		bbier_tmp = bbier_tmp->bbier_next;
	}
	if (last_bbier)
	{
		if (last_bbier != bbier)
		{
		    last_bbier->bbier_next = bbier;
		}
	}

	debugf("=====================bbier_insert_route END\n");

	return;
}

void bbier_del_route(unsigned char *prefix, int ae)
{
    struct bbier_route *bbier_tmp, *last_bbier = NULL;
    int i;

    if (bbier_global == NULL)
    	return;

	bbier_tmp = bbier_global;
	if (ae ==1)
	{
	    i = memcmp(prefix, bbier_tmp->ipv4_prefix, 4);
	    if (i == 0)
	    {
	    	bbier_global = bbier_tmp->bbier_next;
	    	bbier_delallin_bbier(bbier_tmp);
	    	free(bbier_tmp);
	    	return;
	    }
		last_bbier = bbier_tmp;
		bbier_tmp = bbier_tmp->bbier_next;
	    while (bbier_tmp)
	    {
	    	i = memcmp(bbier_tmp->ipv4_prefix, prefix, 4);
	    	if (i == 0)
	    	{
	    		last_bbier->bbier_next = bbier_tmp->bbier_next;
	    		bbier_delallin_bbier(bbier_tmp);
	    		free(bbier_tmp);
	    		return;
	    	}
	    	last_bbier = bbier_tmp;
	        bbier_tmp = bbier_tmp->bbier_next;
	    }
	}
	return;
}

int bbier_insert_bbier_info(struct bbier_route *bbier, struct bbier_route_sub *bbrs_new, int ae)
{
	struct bbier_route_sub *last_bbrs=NULL, *bbrs = NULL;

	if (!bbier || !bbrs_new) {
    	return 0;
	}

	debugf("==============================bbier_insert_bbier_info, subdomain_id: %d, bfr-id: %d, ae: %d.\n",
	            bbrs_new->subdomain_id, bbrs_new->bfr_id, ae);

	if (ae == 1)
    {
    	if (bbier->ipv4_sdcount < 1)
    	{
    		bbier->ipv4_bbrs = bbrs_new;
    		bbier->ipv4_sdcount++;
    	}
    	else
    	{
    	    last_bbrs = NULL;
    	    bbrs = bbier->ipv4_bbrs;

    	    while (bbrs)
    	    {
    	   	    /*if (bbrs_new->subdomain_id < bbrs->subdomian_id) {
    	     	    bbrs_new->bbrs_next = bbrs;
    	       	    if (last_bbrs)
    	       	        last_bbrs->bbrs_next = bbrs_new;
    	       	    else
    	       		    bbier->bbrs = bbrs_new;

    	       	    insert_success = 1;
    	        }*/

    	        last_bbrs = bbrs;
    	        bbrs = bbrs->bbrs_next;
    	    }
    	    if (last_bbrs)
    	    {
    		    if (last_bbrs != bbrs_new)
    		    {
    		    	last_bbrs->bbrs_next = bbrs_new;
    		    	bbier->ipv4_sdcount++;
    		    }
    	    }
    	}
    }
    else if (ae == 2)
    {
    	if (bbier->ipv6_sdcount < 1)
    	{
    		bbier->ipv6_bbrs = bbrs_new;
    		bbier->ipv6_sdcount++;
    	}
    	else
    	{
        	last_bbrs = NULL;
        	bbrs = bbier->ipv6_bbrs;

        	while (bbrs)
         	{
    	        last_bbrs = bbrs;
    	        bbrs = bbrs->bbrs_next;
    	    }
    	    if (last_bbrs)
    	    {
    	        if (last_bbrs != bbrs_new)
    	        {
    	        	last_bbrs->bbrs_next = bbrs_new;
    	        	bbier->ipv6_sdcount++;
    	        }
    	    }
    	}
    }

    return 1;
}

struct bbier_route_mpls *
bbier_locate_bbrsm(struct bbier_route_sub *bbrs, unsigned char bsl)
{
	struct bbier_route_mpls *bbrsm = NULL;

	if (!bbrs)
		return NULL;

	bbrsm = bbrs->bb_mpls;
	while (bbrsm)
	{
		assert(bbrsm != bbrsm->bbrsm_next);

		if (bbrsm->bsl == bsl)
			break;

		bbrsm = bbrsm->bbrsm_next;
	}
	return bbrsm;
}

int bbier_insert_bbrsm(struct bbier_route_sub *bbrs, struct bbier_route_mpls *bbrsm_new)
{
	struct bbier_route_mpls *last_bbrsm, *bbrsm=NULL;

	if (bbrs->bbmpls_count < 1)
	{
		bbrs->bb_mpls = bbrsm_new;
		bbrs->bbmpls_count++;
		return 1;
	}

    last_bbrsm = NULL;
    bbrsm = bbrs->bb_mpls;

    while (bbrsm)
    {
    	/*if (bbrsm_new->bsl < bbrsm->bsl) {
        	bbrsm_new->bbrsm_next = bbrsm;*/
    	last_bbrsm = bbrsm;
    	bbrsm = bbrsm->bbrsm_next;
    }
    if (last_bbrsm)
    {
        if (last_bbrsm != bbrsm_new)
        {
        	last_bbrsm->bbrsm_next = bbrsm_new;
        	bbrs->bbmpls_count++;
        }
    }
    else
    {
        bbrs->bb_mpls = bbrsm_new;
        bbrs->bbmpls_count++;
    }
    return 1;
}

struct bbier_route_sub *
bbier_search_subdomain_id(struct bbier_route *bbier, unsigned char subid, int ae)
{
	struct bbier_route_sub *bbrs=NULL;

	if (ae == 1)
	{
		if (bbier->ipv4_sdcount < 1)
			return NULL;

		bbrs = bbier->ipv4_bbrs;
		while (bbrs)
		{
			assert(bbrs != bbrs->bbrs_next);
			if (bbrs->subdomain_id == subid)
				return bbrs;

			bbrs = bbrs->bbrs_next;
		}
	}
	else if (ae == 2)
	{
		if (bbier->ipv6_sdcount < 1)
			return NULL;

		bbrs = bbier->ipv6_bbrs;
		while (bbrs)
		{
			assert(bbrs != bbrs->bbrs_next);
			if (bbrs->subdomain_id == subid)
				return bbrs;

			bbrs = bbrs->bbrs_next;
		}
	}

	debugf("***********************bbier_search_subdomain_id: Found Nothing.\n");

	return NULL;
}

struct bbier_route *
bbier_check_prefix_local(int ae, unsigned char *src_prefix, unsigned char *flag)
{
    struct bbier_route *bbier_tmp = NULL;
    int i=0;

    if (!bbier_global)
    	return NULL;

	bbier_tmp = bbier_global;
	*flag = 0;

    if (ae == 1)
    {
    	while (bbier_tmp)
    	{
    		assert(bbier_tmp != bbier_tmp->bbier_next);
    		i = memcmp(src_prefix, &bbier_tmp->ipv4_prefix, 4);
    		if (i == 0)
    		{
    			if (bbier_tmp->local_flag == 1)
    			{
    				*flag = 1;
    			}
    			return bbier_tmp;
    		}
    		bbier_tmp = bbier_tmp->bbier_next;
    	}
    }
    else if (ae == 2)
    {
    	while (bbier_tmp)
    	{
    		assert(bbier_tmp != bbier_tmp->bbier_next);
    		i = memcmp(src_prefix, &bbier_tmp->ipv6_prefix, 16);
    	    if (i == 0)
    	    {
    	        if (bbier_tmp->local_flag == 1)
    	        {
    	            *flag = 1;
    	        }
    	        return bbier_tmp;
    	    }
    	    bbier_tmp = bbier_tmp->bbier_next;
    	}
    }
    return NULL;
}

int
parse_bbier_info_subtlv(const unsigned char *pp, int ae, unsigned char *src_prefix)
{
    const unsigned char *p;
    int i=0, rtn = 0;
    unsigned char flag = 0;
    unsigned char type, len, total_len, /*rev,*/ subid  = 0;
    unsigned short bid = 0;
    struct bbier_route_sub *bbrs_existed = NULL, *bbrs=NULL, *bbrs_new = NULL;
    unsigned char mpls_lbl_rangesize = 0;
    unsigned char mpls_bsl = 0;
    unsigned int mpls_label = 0, mix_mpls = 0, temp_mpls = 0;
    unsigned char bsl_len = 0;
    struct bbier_route_mpls *bbrsm=NULL, *bbrsm_existed = NULL;
    struct bbier_route *bbier = NULL;
    unsigned int tt_prefix = 0;

    if ((ae != 1) && (ae != 2))
        return 0;

    if (!pp)
        return -1;

    p = pp;

    if (ae == 1)
    {
        tt_prefix = *(unsigned int *)(&src_prefix);
        debugf("***********************parse_bbier_info_subtlv: type: %d, total_len: %d, prefix: 0x%x.\n", p[0], p[1], tt_prefix);
    }

    type = p[0];
    if(type != SUBTLV_BABEL_BIER_INFO)
        return -1;

    total_len = p[1];
    if (total_len < 3) /*BIER base info, include reserved, subdomain_id and bfr_id*/
    	return -1;

    i = 0;
    len = 0;
    while(i < total_len) {

    	type = p[i];
    	len = p[i+1];

    	debugf("***********************parse_bbier_info_subtlv: type: %d, length: %d, i: %d.\n", type, len, i);

    	if(i + len > total_len)
            goto fail;

    	if (type == SUBTLV_BABEL_BIER_INFO) {
    		len = 4;
            /*rev = p[i+2];*/
            subid = p[i+3];
            bid = *(unsigned short *)(&p[i+4]);

            debugf("***********************parse_bbier_info_subtlv: INFO, sid: %d, bid: %d.\n", subid, bid);
            /*if the src_prefix is local configuration*/
            bbier = NULL;
           	bbier = bbier_check_prefix_local(ae, src_prefix, &flag);
           	if (flag == 1) {
           		fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO error: the prefix received is own.\n");
           		goto fail;
            }

           	if (!bbier) {
           	    bbier = (struct bbier_route *)malloc(sizeof(struct bbier_route));
           		if (!bbier) {
           			fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO error: No enough memory.\n");
           			return -1;
           		}
           		memset(bbier, 0, sizeof(struct bbier_route));
           		bbier->ae_flag = ae;
           		if (ae == 1)
           		    memcpy(bbier->ipv4_prefix, src_prefix, 4);
           		else if (ae == 2)
           			memcpy(bbier->ipv6_prefix, src_prefix, 16);
           	}

           	bbrs_existed = NULL;
            bbrs_existed =  bbier_search_subdomain_id(bbier, subid, ae);
            if ((bbrs_existed) && (bbrs_existed->bfr_id != bid)) {
            	fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO UPDATE: BFR-id update. subdomian_id %d bfr-id %d.\n", subid, bid);
            	bbier->change_flag = 1;
            }

            if (!bbrs_existed)
            {
           	    bbrs_new = malloc(sizeof(struct bbier_route_sub));
            	if (!bbrs_new) {
            	    fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO error: No enough memory.\n");
            	    return -1;
            	}
            	memset(bbrs_new, 0, sizeof(struct bbier_route_sub));
            	bbrs_new->subdomain_id = subid;
            	bbrs_new->bfr_id = bid;

            	rtn = bbier_insert_bbier_info(bbier, bbrs_new, ae);
                if (rtn < 0) {
                   fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO error: subdomian_id %d duplicated.\n", subid);
                   goto fail;
                }
                bbrs_existed = bbrs_new;
            }

            bbier_insert_route(bbier);

            bbrs = bbrs_existed;
    	}
    	else if (type == BABEL_BIER_MPLS_SUBSUBTLV)
    	{
    		len = 4;
    		debugf("***********************parse_bbier_info_subtlv: BABEL_BIER_MPLS_SUBSUBTLV len: %d, i: %d.\n", len, i);
    		mpls_lbl_rangesize = p[i+2];
    		mix_mpls = *(unsigned int *)(&p[i+2]);

    		temp_mpls = mix_mpls;
    		temp_mpls = temp_mpls >> 8;
    		temp_mpls = temp_mpls & 0xF;
    		mpls_bsl = (unsigned char)temp_mpls;

    		temp_mpls = mix_mpls;
    		temp_mpls = temp_mpls >> 12;
    		temp_mpls = temp_mpls & 0xFFFFF;
    		mpls_label = temp_mpls;

    		/*mpls_bsl = (mix_mpls>>8)&0xF;
    		mpls_label = (mix_mpls>>12)&0xFFFFF;*/

            if (bbrs)
            {
            	bbrsm_existed =  bbier_locate_bbrsm(bbrs, mpls_bsl);
                if (!bbrsm_existed) {
                	bbrsm = malloc(sizeof(struct bbier_route_mpls));
                	if (!bbrsm) {
                		fprintf(stderr, "No enough memory: alloc bbier_route_mpls.\n");
                		return -1;
                	}
                	memset(bbrsm, 0, sizeof(struct bbier_route_mpls));
                	bbrsm->bsl = mpls_bsl;

                	bbier_insert_bbrsm(bbrs, bbrsm);
                }
                else {
                	bbrsm = bbrsm_existed;
                }
                bbrsm->label_range_size = mpls_lbl_rangesize;
                bbrsm->label_value = mpls_label;
                bbrsm->mix_value = mix_mpls;
            }
    	}
    	else if (type == BABEL_BIER_BSL_SUBSUBTLV)
    	{
    		len = 0;
    		bsl_len = p[i+1];

    		if (bsl_len == 0){
    			fprintf(stderr, "Parse BABEL_BIER_BSL_SUBSUBTLV error: bsl_conversion 0.\n");
    			goto fail;
    		}

            if (bbrs)
            	bbrs->bsl_conversion = bsl_len;
        }
    	else {
	    	fprintf(stderr, "Parse SUBTLV_BABEL_BIER_INFO error: Unknown subsub type %d.\n", type);
	    	goto fail;
	    }
    	i += len;
    	i += 2;
    }

    return 1;

fail:
    fprintf(stderr, "Execute BIER sub-TLV Error.\n");
    if (bbier)
        bbier_release_bbier(bbier);
    return -1;
}

void bbier_release_bbrsm(struct bbier_route_sub *bbrs, struct bbier_route_mpls *bbrsm)
{
	struct bbier_route_mpls *temp_bbrsm = NULL, *last_bbrsm = NULL;

	if (!bbrs || !bbrsm)
		return;

	if (bbrs->bb_mpls == bbrsm) /*The first one*/
	{
		bbrs->bb_mpls = bbrsm->bbrsm_next;
		bbrs->bbmpls_count--;
		free(bbrsm);
		return;
	}

	temp_bbrsm = bbrs->bb_mpls;
	while (temp_bbrsm)
	{
		last_bbrsm = temp_bbrsm;
		if (temp_bbrsm == bbrsm)
        {
            bbrs->bbmpls_count--;
            last_bbrsm->bbrsm_next = bbrsm->bbrsm_next;
            free(bbrsm);
            return;
        }
        temp_bbrsm = temp_bbrsm->bbrsm_next;
	}
	return;
}

void bbier_delallin_bbrs(int ae, struct bbier_route_sub *bbrs)
{
	struct bbier_route_mpls *temp_bbrsm = NULL, *next_bbrsm = NULL;

	if (!bbrs)
		return;

	temp_bbrsm = bbrs->bb_mpls;
	while (temp_bbrsm)
	{
		next_bbrsm = temp_bbrsm->bbrsm_next;
		bbier_release_bbrsm(bbrs, temp_bbrsm);
		temp_bbrsm = next_bbrsm;
	}

	bbier_release_bbrsm(bbrs, bbrs->bb_mpls);

	return;
}

void bbier_release_bbrs(struct bbier_route *bbier, struct bbier_route_sub *bbrs)
{
	struct bbier_route_sub *temp_bbrs = NULL, *last_bbrs = NULL;

	if (!bbier || !bbrs)
		return;

	if (bbier->ae_flag == 1)
	{
		if (bbier->ipv4_bbrs == bbrs)
		{
			bbier->ipv4_sdcount--;
			bbier->ipv4_bbrs = bbrs->bbrs_next;
			free(bbrs);
			return;
		}

		temp_bbrs = bbier->ipv4_bbrs;
		while (temp_bbrs)
		{
		    last_bbrs = temp_bbrs;
			if (temp_bbrs == bbrs)
		    {
		        bbier->ipv4_sdcount--;
		        last_bbrs->bbrs_next = bbrs->bbrs_next;
		        bbier_delallin_bbrs(1, bbrs);
		        free(bbrs);
		        return;
		    }
		    temp_bbrs = temp_bbrs->bbrs_next;
		}
	}
	else if (bbier->ae_flag == 2)
	{
		if (bbier->ipv6_bbrs == bbrs)
		{
			bbier->ipv6_sdcount--;
			bbier->ipv6_bbrs = bbrs->bbrs_next;
			free(bbrs);
			return;
		}

		temp_bbrs = bbier->ipv6_bbrs;
		while (temp_bbrs)
		{
			last_bbrs = temp_bbrs;
			if (temp_bbrs == bbrs)
		    {
		        bbier->ipv6_sdcount--;
		        last_bbrs->bbrs_next = bbrs->bbrs_next;
		        bbier_delallin_bbrs(2, bbrs);
		        free(bbrs);
		        return;
		    }
		    temp_bbrs = temp_bbrs->bbrs_next;
		}
	}

    return;
}

void bbier_delallin_bbier(struct bbier_route *bbier)
{
	struct bbier_route_sub *temp_bbrs, *next_bbrs = NULL;

	if (!bbier)
		return;

	if (bbier->ae_flag == 1)
	{
		temp_bbrs = bbier->ipv4_bbrs;
		while (temp_bbrs)
		{
			next_bbrs = temp_bbrs->bbrs_next;
			bbier_delallin_bbrs(1, temp_bbrs);
			temp_bbrs = next_bbrs;
		}
		bbier_release_bbrs(bbier, bbier->ipv4_bbrs);
		bbier->ipv4_bbrs = NULL;
	}
	else if (bbier->ae_flag == 2)
	{
		temp_bbrs = bbier->ipv6_bbrs;
		while (temp_bbrs)
		{
			next_bbrs = temp_bbrs->bbrs_next;
			bbier_delallin_bbrs(2, temp_bbrs);
			temp_bbrs = next_bbrs;
		}
		bbier_release_bbrs(bbier, bbier->ipv6_bbrs);
		bbier->ipv6_bbrs = NULL;
	}

	bbier->ipv4_sdcount = 0;
	bbier->ipv6_sdcount = 0;
	bbier->ipv4_bbrs = NULL;
	bbier->ipv6_bbrs = NULL;

	return;
}

void bbier_release_bbier(struct bbier_route *bbier)
{
	struct bbier_route *temp_bbier = NULL;

	if ((!bbier) || (!bbier_global))
		return;

	debugf("=====================bbier_release_bbier BEGIN\n");
	if (bbier == bbier_global)
	{
        bbier_global = bbier->bbier_next; /*delete the first one*/
	}
	else
	{
        temp_bbier = bbier_global;
        while (temp_bbier)
	    {
        	if (temp_bbier->bbier_next == bbier)
		    {
			    temp_bbier->bbier_next = bbier->bbier_next;
			    break;
		    }
		    temp_bbier = temp_bbier->bbier_next;
	    }
	}

	bbier_delallin_bbier(bbier);

    return;
}


void bbier_notify_route(struct babel_route *route)
{
    /*notify BIER the information received.*/
    /*Now it is empty.*/
	/*If the route is retracted, delete the associated bbier*/
	return;
}

struct bbier_route *
bbier_locate_prefix(int ae, const unsigned char *src_prefix)
{
	struct bbier_route *next_bbier = NULL, *tmp_bbier = NULL;
	int i=0;

	if (!bbier_global) {
		debugf("=====================ERROR: bbier_global NULL\n");
		return NULL;
	}

	tmp_bbier = bbier_global;
	if (ae == 1)
	{
        while (tmp_bbier)
        {
        	assert(tmp_bbier != tmp_bbier->bbier_next);
       		next_bbier = tmp_bbier->bbier_next;

       		i = memcmp(src_prefix, tmp_bbier->ipv4_prefix, 4);
        	if (i == 0)
        	{
        		return tmp_bbier;
        	}

        	tmp_bbier = next_bbier;
        }
	}
	else if (ae == 2)
	{
		while (tmp_bbier)
		{
		    i = memcmp(src_prefix, tmp_bbier->ipv6_prefix, 16);
		    if (i == 0)
		    {
		        return tmp_bbier;
		    }

		    tmp_bbier = tmp_bbier->bbier_next;
		}
	}
	debugf("=====================bbier_locate_prefix END, NO marching\n");

	return NULL;
}

void bbier_del_bbier_route(struct babel_route *route)
{
	int ae = 0;
	struct bbier_route *bbier = NULL;
	if (!route)
		return;
	if (!route->src)
		return;

	if (route->src->plen >= 96)
		ae = 1;
	else
		ae = 2;

	bbier = bbier_locate_prefix(ae, route->src->prefix);
	if (!bbier)
		return;

	bbier_release_bbier(bbier);
}

int bbier_call_fillin(unsigned char *buf, const unsigned char *prefix, unsigned char len)
{
    int rtn_len = 0;

    if (!bbier_global){
    	return 0;
    }

    debugf("=====================bbier_call_fillin BEGIN\n");

	if (len <= 32)
	{
		debugf("=====================bbier_call_fillin IPv4\n");
		rtn_len = bbier_fillin_info_subtlv(buf, 1, prefix);
	}
	else
	{
		debugf("=====================bbier_call_fillin IPv6\n");
		rtn_len = bbier_fillin_info_subtlv(buf, 2, prefix);
	}

	debugf("=====================bbier_call_fillin END\n");
	return rtn_len;
}

int bbier_fillin_info_subtlv(unsigned char *pp, int ae, const unsigned char *prefix/*, int *length*/)
{
    struct bbier_route *bbier = NULL;
    int len=0;
    unsigned char *p=NULL;
    struct bbier_route_sub *bbrs = NULL;
    struct bbier_route_mpls *bbrsm = NULL;
    unsigned int value1, value2, value3, value = 0;

    if (!pp)
    	return 0;

    p = pp;

    debugf("=====================bbier_fillin_info_subtlv BEGIN\n");

    bbier = bbier_locate_prefix(ae, prefix);

    if (bbier == NULL)
    {
    	return 0;
    }

    len = 0;
    if (ae == 1)
    {
        bbrs = bbier->ipv4_bbrs;
        while (bbrs)
        {
        	p[0] = SUBTLV_BABEL_BIER_INFO;
        	p[2] = 0;
        	p[3] = bbrs->subdomain_id;
            *(unsigned short *)(&p[4]) = bbrs->bfr_id;

            p = p+6;
            len = len+6;
            bbrsm = bbrs->bb_mpls;
            while (bbrsm)
            {
            	debugf("=====================bbier_fillin_info_subtlv lrange: %d, bsl: %d, label: %d\n", bbrsm->label_range_size, bbrsm->bsl, bbrsm->label_value);
            	p[0] = BABEL_BIER_MPLS_SUBSUBTLV;
            	p[1] = 4;

            	value1 = (unsigned int)bbrsm->label_range_size;
            	value2 = (unsigned int)bbrsm->bsl;
            	value2 = value2 << 8;
            	value2 = value2 & 0x00000F00;
            	value3 = bbrsm->label_value;
            	value3 = value3 << 12;
            	value3 = value3 & 0xFFFFF000;

            	value = value1 + value2 + value3;
            	debugf("=====================bbier_fillin_info_subtlv value: %x\n", value);
                *(unsigned int *)(&p[2]) = value;
                p = p+6;
                len = len+6;
                bbrsm = bbrsm->bbrsm_next;
            }
            if (bbrs->bsl_conversion != 0)
            {
            	p[0] = BABEL_BIER_BSL_SUBSUBTLV;
            	*(unsigned short *)(&p[1]) = bbrs->bsl_conversion;
            	len = len+2;
            }
            pp[1] = len-2; /*exclude type and length*/
            bbrs = bbrs->bbrs_next;
        }
    }
    else if (ae == 2)
    {
        bbrs = bbier->ipv6_bbrs;
        while (bbrs)
        {
        	p[0] = SUBTLV_BABEL_BIER_INFO;
        	p[2] = 0;
        	p[3] = bbrs->subdomain_id;
            *(unsigned short *)(&p[4]) = bbrs->bfr_id;

            p = p+6;
            len = len+6;
            bbrsm = bbrs->bb_mpls;
            while (bbrsm)
            {
            	debugf("=====================bbier_fillin_info_subtlv lrange: %d, bsl: %d, label: %d\n", bbrsm->label_range_size, bbrsm->bsl, bbrsm->label_value);
            	p[0] = BABEL_BIER_MPLS_SUBSUBTLV;
            	p[1] = 4;

            	value1 = (unsigned int)bbrsm->label_range_size;
            	value2 = (unsigned int)bbrsm->bsl;
            	value2 = value2 << 8;
            	value2 = value2 & 0x00000F00;
            	value3 = bbrsm->label_value;
            	value3 = value3 << 12;
            	value3 = value3 & 0xFFFFF000;

            	value = value1 + value2 + value3;
            	debugf("=====================bbier_fillin_info_subtlv value: %x\n", value);
                *(unsigned int *)(&p[2]) = value;
                p = p+6;
                len = len+6;
                bbrsm = bbrsm->bbrsm_next;
            }
            if (bbrs->bsl_conversion != 0)
            {
            	p[0] = BABEL_BIER_BSL_SUBSUBTLV;
            	*(unsigned short *)(&p[1]) = bbrs->bsl_conversion;
            	len = len+2;
            }
            pp[1] = len-2; /*exclude type and length*/
            bbrs = bbrs->bbrs_next;
        }
    }

    debugf("=====================bbier_fillin_info_subtlv END\n");
	return len;
}
