/*
 * Open AnyConnect (SSL + DTLS) client
 *
 * © 2008 David Woodhouse <dwmw2@infradead.org>
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <openssl/rand.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "anyconnect.h"

/* The master-secret is generated randomly by the client. The server
   responds with a DTLS Session-ID. These are enough to 'resume' the DTLS
   session, bypassing all the initial setup of a normal DTLS connection.
   Or you can just send traffic over the HTTPS connection... */

int verbose = 0;

static struct option long_options[] = {
	{"cookie", 1, 0, 'c'},
	{"host", 1, 0, 'h'},
	{"mtu", 1, 0, 'm'},
	{"verbose", 1, 0, 'v'},
	{"deflate", 0, 0, 'd'},
	{"useragent", 1, 0, 'u'},
};

int main(int argc, char **argv)
{
	struct anyconnect_info *vpninfo;
	struct utsname utsbuf;
	int optind;
	int opt;

	vpn_init_openssl();

	vpninfo = malloc(sizeof(*vpninfo));
	if (!vpninfo) {
		fprintf(stderr, "Failed to allocate vpninfo structure\n");
		exit(1);
	}
	memset(vpninfo, 0, sizeof(*vpninfo));

	/* Set up some defaults */
	vpninfo->tun_fd = vpninfo->ssl_fd = vpninfo->dtls_fd = -1;
	vpninfo->useragent = "Open AnyConnect VPN Agent v0.01";
	vpninfo->mtu = 1406;
	if (RAND_bytes(vpninfo->dtls_secret, sizeof(vpninfo->dtls_secret)) != 1) {
		fprintf(stderr, "Failed to initialise DTLS secret\n");
		exit(1);
	}
	if (!uname(&utsbuf))
		vpninfo->localname = utsbuf.nodename;
	else
		vpninfo->localname = "localhost";

	/* While we get it working, make it slightly easier... and don't forget */
	fprintf(stderr, "HACKING DTLS SECRET TO 0x5A5A5A...\n");
	memset(vpninfo->dtls_secret, 0x5a, sizeof(vpninfo->dtls_secret));

	while ((opt = getopt_long(argc, argv, "c:h:vdu:", long_options, &optind))) {
		if (opt < 0)
			break;

		switch (opt) {
		case 'c':
			vpninfo->cookie = optarg;
			break;

		case 'h':
			vpninfo->hostname = optarg;
			break;

		case 'm':
			vpninfo->mtu = atol(optarg);
			if (vpninfo->mtu < 576) {
				fprintf(stderr, "MTU %d too small\n", vpninfo->mtu);
				vpninfo->mtu = 576;
			}
			break;

		case 'u':
			vpninfo->useragent = optarg;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'd':
			fprintf(stderr, "Deflate not yet supported\n");
			//vpninfo->deflate = 1;
			break;
		}
	}
	if (!vpninfo->hostname || !vpninfo->cookie) {
		fprintf(stderr, "Need -h hostname, -c cookie\n");
		exit(1);
	}

	if (make_ssl_connection(vpninfo)) {
		fprintf(stderr, "Creating SSL connection failed\n");
		exit(1);
	}
	
	if (setup_tun(vpninfo)) {
		fprintf(stderr, "Set up tun device failed\n");
		exit(1);
	}

	if (setup_dtls(vpninfo))
		fprintf(stderr, "Set up DTLS failed; using SSL instead\n");

	printf("Connected\n");
	vpn_mainloop(vpninfo);
	exit(1);
}