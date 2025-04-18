/*
 * ZMap Copyright 2013 Regents of the University of Michigan
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include "../../lib/includes.h"
#include "probe_modules.h"
#include "module_udp.h"
#include "module_ntp.h"
#include "packet.h"
#include "logger.h"

#define MAX_NTP_PAYLOAD_LEN 1472
#define ICMP_UNREACH_HEADER_SIZE 8

probe_module_t module_ntp;

static int num_ports;
#define SOURCE_PORT_VALIDATION_MODULE_DEFAULT true; // default to validating source port
static bool should_validate_src_port = SOURCE_PORT_VALIDATION_MODULE_DEFAULT

int ntp_global_initialize(struct state_conf *conf)
{
	num_ports = conf->source_port_last - conf->source_port_first + 1;
	if (conf->validate_source_port_override == VALIDATE_SRC_PORT_DISABLE_OVERRIDE) {
		log_debug("ntp", "disabling source port validation");
		should_validate_src_port = false;
	}
	return udp_global_initialize(conf);
}

int ntp_validate_packet(const struct ip *ip_hdr, uint32_t len, uint32_t *src_ip,
			uint32_t *validation, const struct port_conf *ports)
{
	return udp_do_validate_packet(ip_hdr, len, src_ip, validation,
				      num_ports, should_validate_src_port, ports);
}

void ntp_process_packet(const u_char *packet, UNUSED uint32_t len,
			fieldset_t *fs, UNUSED uint32_t *validation,
			UNUSED struct timespec ts)
{
	struct ip *ip_hdr = (struct ip *)&packet[sizeof(struct ether_header)];
	uint64_t temp64;
	uint8_t temp8;
	uint32_t temp32;

	if (ip_hdr->ip_p == IPPROTO_UDP) {
		struct udphdr *udp =
		    (struct udphdr *)((char *)ip_hdr + ip_hdr->ip_hl * 4);
		uint8_t *ptr = (uint8_t *)&udp[1];

		fs_add_string(fs, "classification", (char *)"ntp", 0);
		fs_add_bool(fs, "success", 1);
		fs_add_uint64(fs, "sport", ntohs(udp->uh_sport));
		fs_add_uint64(fs, "dport", ntohs(udp->uh_dport));
		fs_add_null(fs, "icmp_responder");
		fs_add_null(fs, "icmp_type");
		fs_add_null(fs, "icmp_code");
		fs_add_null(fs, "icmp_unreach_str");

		if (len > 90) {
			temp8 = *((uint8_t *)ptr);
			fs_add_uint64(fs, "LI_VN_MODE", temp8);
			temp8 = *((uint8_t *)ptr + 1);
			fs_add_uint64(fs, "stratum", temp8);
			temp8 = *((uint8_t *)ptr + 2);
			fs_add_uint64(fs, "poll", temp8);
			temp8 = *((uint8_t *)ptr + 3);
			fs_add_uint64(fs, "precision", temp8);

			temp32 = *((uint32_t *)ptr + 4);
			fs_add_uint64(fs, "root_delay", temp32);
			temp32 = *((uint32_t *)ptr + 8);
			fs_add_uint64(fs, "root_dispersion", temp32);
			temp32 = *((uint32_t *)ptr + 12);
			fs_add_uint64(fs, "reference_clock_identifier", temp32);

			temp64 = *((uint64_t *)ptr + 16);
			fs_add_uint64(fs, "reference_timestamp", temp64);
			temp64 = *((uint64_t *)ptr + 24);
			fs_add_uint64(fs, "originate_timestamp", temp64);
			temp64 = *((uint64_t *)ptr + 32);
			fs_add_uint64(fs, "receive_timestamp", temp64);
			temp64 = *((uint64_t *)ptr + 39);
			fs_add_uint64(fs, "transmit_timestamp", temp64);
		} else {
			fs_add_null(fs, "LI_VN_MODE");
			fs_add_null(fs, "stratum");
			fs_add_null(fs, "poll");
			fs_add_null(fs, "precision");
			fs_add_null(fs, "root_delay");
			fs_add_null(fs, "root_dispersion");
			fs_add_null(fs, "reference_clock_identifier");
			fs_add_null(fs, "reference_timestamp");
			fs_add_null(fs, "originate_timestamp");
			fs_add_null(fs, "receive_timestamp");
			fs_add_null(fs, "transmit_timestamp");
		}
	} else if (ip_hdr->ip_p == IPPROTO_ICMP) {
		struct icmp *icmp =
		    (struct icmp *)((char *)ip_hdr + ip_hdr->ip_hl * 4);
		struct ip *ip_inner =
		    (struct ip *)((char *)icmp + ICMP_UNREACH_HEADER_SIZE);

		fs_modify_string(fs, "saddr",
				 make_ip_str(ip_inner->ip_dst.s_addr), 1);
		fs_add_constchar(fs, "classification", "icmp");
		fs_add_bool(fs, "success", 0);
		fs_add_null(fs, "sport");
		fs_add_null(fs, "dport");
		fs_add_string(fs, "icmp_responder",
			      make_ip_str(ip_hdr->ip_src.s_addr), 1);
		fs_add_uint64(fs, "icmp_type", icmp->icmp_type);
		fs_add_uint64(fs, "icmp_code", icmp->icmp_code);
		fs_add_null(fs, "icmp_unreach_str");

		fs_add_null(fs, "LI_VN_MODE");
		fs_add_null(fs, "stratum");
		fs_add_null(fs, "poll");
		fs_add_null(fs, "precision");
		fs_add_null(fs, "root_delay");
		fs_add_null(fs, "root_dispersion");
		fs_add_null(fs, "reference_clock_identifier");
		fs_add_null(fs, "reference_timestamp");
		fs_add_null(fs, "originate_timestamp");
		fs_add_null(fs, "receive_timestamp");
		fs_add_null(fs, "transmit_timestamp");

	} else {
		fs_add_constchar(fs, "classification", "other");
		fs_add_bool(fs, "success", 0);
		fs_add_null(fs, "sport");
		fs_add_null(fs, "dport");
		fs_add_null(fs, "icmp_responder");
		fs_add_null(fs, "icmp_type");
		fs_add_null(fs, "icmp_code");
		fs_add_null(fs, "icmp_unreach_str");
		fs_add_null(fs, "LI_VN_MODE");
		fs_add_null(fs, "stratum");
		fs_add_null(fs, "poll");
		fs_add_null(fs, "precision");
		fs_add_null(fs, "root_delay");
		fs_add_null(fs, "root_dispersion");
		fs_add_null(fs, "reference_clock_identifier");
		fs_add_null(fs, "reference_timestamp");
		fs_add_null(fs, "originate_timestamp");
		fs_add_null(fs, "receive_timestamp");
		fs_add_null(fs, "transmit_timestamp");
	}
}

int ntp_init_perthread(void **arg)
{
	uint32_t seed = aesrand_getword(zconf.aes);
	aesrand_t *aes = aesrand_init_from_seed(seed);
	*arg = aes;

	return EXIT_SUCCESS;
}

int ntp_prepare_packet(void *buf, macaddr_t *src, macaddr_t *gw, UNUSED void *arg)
{
	memset(buf, 0, MAX_PACKET_SIZE);
	struct ether_header *eth_header = (struct ether_header *)buf;
	make_eth_header(eth_header, src, gw);
	struct ip *ip_header = (struct ip *)(&eth_header[1]);
	uint16_t len = htons(sizeof(struct ip) + sizeof(struct udphdr) +
			     sizeof(struct ntphdr));
	make_ip_header(ip_header, IPPROTO_UDP, len);

	struct udphdr *udp_header = (struct udphdr *)(&ip_header[1]);
	struct ntphdr *ntp_header = (struct ntphdr *)(&udp_header[1]);
	ntp_header->LI_VN_MODE = 227;
	len = sizeof(struct udphdr) + sizeof(struct ntphdr);

	make_udp_header(udp_header, len);

	// TODO(dadrian): Should this have a payload? It was being set incorrectly.
	size_t header_len = sizeof(struct ether_header) + sizeof(struct ip) +
			    sizeof(struct udphdr) + sizeof(struct ntphdr);
	module_ntp.max_packet_length = header_len;

	return EXIT_SUCCESS;
}

void ntp_print_packet(FILE *fp, void *packet)
{

	struct ether_header *ethh = (struct ether_header *)packet;
	struct ip *iph = (struct ip *)&ethh[1];
	struct udphdr *udph = (struct udphdr *)(iph + 4 * iph->ip_hl);
	struct ntphdr *ntph = (struct ntphdr *)&udph[1];
	fprintf(fp, "ntp { LI_VN_MODE: %u | stratum: %u | poll: %u }\n",
		ntph->LI_VN_MODE, ntph->stratum, ntph->poll);
	fprintf(fp, "udp { source: %u | dest: %u | checksum: %#04X }\n",
		ntohs(udph->uh_sport), ntohs(udph->uh_dport),
		ntohs(udph->uh_sum));
	fprintf_ip_header(fp, iph);
	fprintf_eth_header(fp, ethh);
	fprintf(fp, PRINT_PACKET_SEP);
}

static fielddef_t fields[] = {
    {.name = "classification",
     .type = "string",
     .desc = "packet classification"},
    {.name = "success",
     .type = "bool",
     .desc = "is  response considered success"},
    {.name = "sport", .type = "int", .desc = "UDP source port"},
    {.name = "dport", .type = "int", .desc = "UDP destination port"},
    ICMP_FIELDSET_FIELDS,
    {.name = "LI_VN_MODE",
     .type = "int",
     .desc = "leap indication, version number, mode"},
    {.name = "stratum", .type = "int", .desc = "stratum"},
    {.name = "poll", .type = "int", .desc = "poll"},
    {.name = "precision", .type = "int", .desc = "precision"},
    {.name = "root_delay", .type = "int", .desc = "root delay"},
    {.name = "root_dispersion", .type = "int", .desc = "root dispersion"},
    {.name = "reference_clock_identifier",
     .type = "int",
     .desc = "code identifying clock reference"},
    {.name = "reference_timestamp",
     .type = "int",
     .desc = "local time at which local clock was last set or corrected"},
    {.name = "originate_timestamp",
     .type = "int",
     .desc = "local time at which request deparated client for service"},
    {.name = "receive_timestamp",
     .type = "int",
     .desc = "local time at which request arrvied at service host"},
    {.name = "transmit_timestamp",
     .type = "int",
     .desc = "local time which reply departed service host for client"},
};

probe_module_t module_ntp = {.name = "ntp",
			     .max_packet_length = 0, // set in init
			     .pcap_filter = "udp || icmp",
			     .pcap_snaplen = 1500,
			     .port_args = 1,
			     .global_initialize = &ntp_global_initialize,
			     .thread_initialize = &ntp_init_perthread,
			     .prepare_packet = &ntp_prepare_packet,
			     .make_packet = &udp_make_packet,
			     .print_packet = &ntp_print_packet,
			     .validate_packet = &ntp_validate_packet,
			     .process_packet = &ntp_process_packet,
			     .close = &udp_global_cleanup,
			     .output_type = OUTPUT_TYPE_STATIC,
			     .fields = fields,
			     .numfields = sizeof(fields) / sizeof(fields[0])};
