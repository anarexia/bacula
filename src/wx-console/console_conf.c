/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Main configuration file parser for Bacula User Agent
 *    some parts may be split into separate files such as
 *    the schedule configuration (sch_config.c).
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_config.c and
 * lib/parse_config.h.
 * These files contain the parser code, some utility
 * routines, and the common store routines (name, int,
 * string).
 *
 *   3. The daemon specific file, which contains the Resource
 * definitions as well as any specific store routines
 * for the resource records.
 *
 *     Kern Sibbald, January MM, September MM
 */

/* _("...") macro returns a wxChar*, so if we need a char*, we need to convert it with:
 * wxString(_("...")).mb_str(*wxConvCurrent) */

/*  Windows debug builds set _DEBUG which is used by wxWidgets to select their
 *  debug memory allocator.  Unfortunately it conflicts with Bacula's SmartAlloc.
 * So we turn _DEBUG off since we aren't interested in things it enables.
 */

#undef _DEBUG

#include "bacula.h"
#include "console_conf.h"

#include <wx/intl.h>

/* Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
int32_t r_first = R_FIRST;
int32_t r_last  = R_LAST;
static RES *sres_head[R_LAST - R_FIRST + 1];
RES **res_head = sres_head;

/* Forward referenced subroutines */


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
    URES res_all;
}
#else
URES res_all;
#endif
int32_t res_all_size = sizeof(res_all);

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/*  Console "globals" */
static RES_ITEM cons_items[] = {
   {"name",        store_name,     ITEM(res_cons.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(res_cons.hdr.desc), 0, 0, 0},
   {"rcfile",      store_dir,      ITEM(res_cons.rc_file), 0, 0, 0},
   {"historyfile", store_dir,      ITEM(res_cons.hist_file), 0, 0, 0},
   {"password",    store_password, ITEM(res_cons.password), 0, ITEM_REQUIRED, 0},
   {"tlsenable",      store_bool,    ITEM(res_cons.tls_enable), 1, 0, 0},
   {"tlsrequire",     store_bool,    ITEM(res_cons.tls_require), 1, 0, 0},
   {"tlscacertificatefile", store_dir, ITEM(res_cons.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir", store_dir,  ITEM(res_cons.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate", store_dir,       ITEM(res_cons.tls_certfile), 0, 0, 0},
   {"tlskey",         store_dir,       ITEM(res_cons.tls_keyfile), 0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};


/*  Director's that we can contact */
static RES_ITEM dir_items[] = {
   {"name",        store_name,     ITEM(res_dir.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(res_dir.hdr.desc), 0, 0, 0},
   {"dirport",     store_pint32,   ITEM(res_dir.DIRport),  0, ITEM_DEFAULT, 9101},
   {"address",     store_str,      ITEM(res_dir.address),  0, 0, 0},
   {"password",    store_password, ITEM(res_dir.password), 0, ITEM_REQUIRED, 0},
   {"tlsenable",      store_bool,    ITEM(res_dir.tls_enable), 1, 0, 0},
   {"tlsrequire",     store_bool,    ITEM(res_dir.tls_require), 1, 0, 0},
   {"tlscacertificatefile", store_dir, ITEM(res_dir.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir", store_dir,  ITEM(res_dir.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate", store_dir,       ITEM(res_dir.tls_certfile), 0, 0, 0},
   {"tlskey",         store_dir,       ITEM(res_dir.tls_keyfile), 0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
RES_TABLE resources[] = {
   {"console",       cons_items,  R_CONSOLE},
   {"director",      dir_items,   R_DIRECTOR},
   {NULL,        NULL,    0}
};


/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   int recurse = 1;

   if (res == NULL) {
      printf(wxString(_("No record for %d %s\n")).mb_str(*wxConvCurrent), type, res_to_str(type));
      return;
   }
   if (type < 0) {            /* no recursion */
      type = - type;
      recurse = 0;
   }
   switch (type) {
   case R_CONSOLE:
      printf(wxString(_("Console: name=%s rcfile=%s histfile=%s\n")).mb_str(*wxConvCurrent), reshdr->name,
             res->res_cons.rc_file, res->res_cons.hist_file);
      break;
   case R_DIRECTOR:
      printf(wxString(_("Director: name=%s address=%s DIRport=%d\n")).mb_str(*wxConvCurrent), reshdr->name,
             res->res_dir.address, res->res_dir.DIRport);
      break;
   default:
      printf(wxString(_("Unknown resource type %d\n")).mb_str(*wxConvCurrent), type);
   }
   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock);
   }
}

/*
 * Free memory of resource.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(RES *sres, int type)
{
   RES *nres;
   URES *res = (URES *)sres;

   if (res == NULL)
      return;

   /* common stuff -- free the resource name */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      bfree(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      bfree(res->res_dir.hdr.desc);
   }

   switch (type) {
   case R_CONSOLE:
      if (res->res_cons.rc_file) {
         bfree(res->res_cons.rc_file);
      }
      if (res->res_cons.hist_file) {
         bfree(res->res_cons.hist_file);
      }
      if (res->res_cons.tls_ctx) { 
         free_tls_context(res->res_cons.tls_ctx);
      }
      if (res->res_cons.tls_ca_certfile) {
         bfree(res->res_cons.tls_ca_certfile);
      }
      if (res->res_cons.tls_ca_certdir) {
         bfree(res->res_cons.tls_ca_certdir);
      }
      if (res->res_cons.tls_certfile) {
         bfree(res->res_cons.tls_certfile);
      }
      if (res->res_cons.tls_keyfile) {
         bfree(res->res_cons.tls_keyfile);
      }
   case R_DIRECTOR:
      if (res->res_dir.address) {
         bfree(res->res_dir.address);
      }
      if (res->res_dir.tls_ctx) { 
         free_tls_context(res->res_dir.tls_ctx);
      }
      if (res->res_dir.tls_ca_certfile) {
         bfree(res->res_dir.tls_ca_certfile);
      }
      if (res->res_dir.tls_ca_certdir) {
         bfree(res->res_dir.tls_ca_certdir);
      }
      if (res->res_dir.tls_certfile) {
         bfree(res->res_dir.tls_certfile);
      }
      if (res->res_dir.tls_keyfile) {
         bfree(res->res_dir.tls_keyfile);
      }
      break;
   default:
         printf(wxString(_("Unknown resource type %d\n")).mb_str(*wxConvCurrent), type);
      }
   /* Common stuff again -- free the resource, recurse to next one */
   bfree(res);
   if (nres) {
      free_resource(nres, type);
   }
}

/* Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
void save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - r_first;
   int i, size;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & ITEM_REQUIRED) {
       if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
               Emsg2(M_ABORT, 0, wxString(_("%s item is required in %s resource, but not found.\n")).mb_str(*wxConvCurrent),
       items[i].name, resources[rindex]);
        }
      }
   }

   /* During pass 2, we looked up pointers to all the resources
    * referrenced in the current resource, , now we
    * must copy their address from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
      /* Resources not containing a resource */
      case R_CONSOLE:
      case R_DIRECTOR:
         break;

      default:
         Emsg1(M_ERROR, 0, wxString(_("Unknown resource type %d\n")).mb_str(*wxConvCurrent), type);
         error = 1;
         break;
      }
      /* Note, the resoure name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_dir.hdr.name) {
         bfree(res_all.res_dir.hdr.name);
         res_all.res_dir.hdr.name = NULL;
      }
      if (res_all.res_dir.hdr.desc) {
         bfree(res_all.res_dir.hdr.desc);
         res_all.res_dir.hdr.desc = NULL;
      }
      return;
   }

   /* The following code is only executed during pass 1 */
   switch (type) {
   case R_CONSOLE:
      size = sizeof(CONRES);
      break;
   case R_DIRECTOR:
      size = sizeof(DIRRES);
      break;
   default:
      printf(wxString(_("Unknown resource type %d\n")).mb_str(*wxConvCurrent), type);
      error = 1;
      size = 1;
      break;
   }
   /* Common */
   if (!error) {
      res = (URES *)bmalloc(size);
      memcpy(res, &res_all, size);
      if (!res_head[rindex]) {
         res_head[rindex] = (RES *)res; /* store first entry */
      } else {
         RES *next, *last;
         for (last=next=res_head[rindex]; next; next=next->next) {
            last = next;
            if (strcmp(next->name, res->res_dir.hdr.name) == 0) {
               Emsg2(M_ERROR_TERM, 0,
                   wxString(_("Attempt to define second %s resource named \"%s\" is not permitted.\n")).mb_str(*wxConvCurrent),
                   resources[rindex].name, res->res_dir.hdr.name);
            }
         }
         last->next = (RES *)res;
            Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type),
                  res->res_dir.hdr.name);
      }
   }
}

bool parse_wxcons_config(CONFIG *config, const char *configfile, LEX_ERROR_HANDLER *scan_error)
{
   config->init(configfile, scan_error, M_ERROR_TERM, (void *)&res_all, res_all_size,
      r_first, r_last, resources, res_head);
   return config->parse_config();
}
