/*
 * IPP processing code for sample IPP server implementation.
 *
 * Copyright © 2014-2022 by the IEEE-ISTO Printer Working Group
 * Copyright © 2010-2019 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#include "ippserver.h"
#ifndef _WIN32
#  include <grp.h>
#endif /* !_WIN32 */
#include <math.h>


/*
 * Local types...
 */

typedef struct server_value_s		/**** Value Validation ****/
{
  const char	*name;			/* Attribute name */
  ipp_tag_t	value_tag,		/* Value tag */
		alt_tag;		/* Alternate value tag, if any */
  int		flags;			/* Validation option flags */
} server_value_t;

#define VALUE_NORMAL	0		/* Normal syntax (1 value) */
#define VALUE_1SETOF	1		/* 1setOf syntax */
#define VALUE_CREATEOP	2		/* Operation attribute for Create-Xxx */


/*
 * Local functions...
 */

static bool		apply_template_attributes(ipp_t *to, ipp_tag_t to_group_tag, server_resource_t *resource, ipp_attribute_t *supported, size_t num_values, server_value_t *values);
static inline int	check_attribute(const char *name, cups_array_t *ra, cups_array_t *pa)
{
  return ((!pa || !cupsArrayFind(pa, (void *)name)) && (!ra || cupsArrayFind(ra, (void *)name)));
}
static void		copy_doc_attributes(server_client_t *client, server_job_t *job, cups_array_t *ra, cups_array_t *pa);
static int		copy_document_uri(server_client_t *client, server_job_t *job, const char *uri);
static void		copy_job_attributes(server_client_t *client, server_job_t *job, cups_array_t *ra, cups_array_t *pa);
static void		copy_printer_attributes(server_client_t *client, server_printer_t *printer, cups_array_t *ra);
static void		copy_printer_state(ipp_t *ipp, server_printer_t *printer, cups_array_t *ra);
static void		copy_resource_attributes(server_client_t *client, server_resource_t *resource, cups_array_t *ra);
static void		copy_subscription_attributes(server_client_t *client, server_subscription_t *sub, cups_array_t *ra, cups_array_t *pa);
static void		copy_system_state(ipp_t *ipp, cups_array_t *ra);
static const char	*detect_format(const unsigned char *header);
static int		filter_cb(server_filter_t *filter, ipp_t *dst, ipp_attribute_t *attr);
static const char	*get_document_uri(server_client_t *client);
static void		ipp_acknowledge_document(server_client_t *client);
static void		ipp_acknowledge_identify_printer(server_client_t *client);
static void		ipp_acknowledge_job(server_client_t *client);
static void		ipp_allocate_printer_resources(server_client_t *client);
static void		ipp_cancel_current_job(server_client_t *client);
static void		ipp_cancel_document(server_client_t *client);
static void		ipp_cancel_job(server_client_t *client);
static void		ipp_cancel_jobs(server_client_t *client);
static void		ipp_cancel_resource(server_client_t *client);
static void		ipp_cancel_subscription(server_client_t *client);
static void		ipp_close_job(server_client_t *client);
static void		ipp_create_job(server_client_t *client);
static void		ipp_create_printer(server_client_t *client);
static void		ipp_create_resource(server_client_t *client);
static void		ipp_create_xxx_subscriptions(server_client_t *client);
static void		ipp_deallocate_printer_resources(server_client_t *client);
static void		ipp_delete_printer(server_client_t *client);
static void		ipp_deregister_output_device(server_client_t *client);
static void		ipp_disable_all_printers(server_client_t *client);
static void		ipp_disable_printer(server_client_t *client);
static void		ipp_enable_all_printers(server_client_t *client);
static void		ipp_enable_printer(server_client_t *client);
static void		ipp_fetch_document(server_client_t *client);
static void		ipp_fetch_job(server_client_t *client);
static void		ipp_get_document_attributes(server_client_t *client);
static void		ipp_get_documents(server_client_t *client);
static void		ipp_get_job_attributes(server_client_t *client);
static void		ipp_get_jobs(server_client_t *client);
static void		ipp_get_notifications(server_client_t *client);
static void		ipp_get_output_device_attributes(server_client_t *client);
static void		ipp_get_printer_attributes(server_client_t *client);
static void		ipp_get_printer_supported_values(server_client_t *client);
static void		ipp_get_printers(server_client_t *client);
static void		ipp_get_resource_attributes(server_client_t *client);
static void		ipp_get_resources(server_client_t *client);
static void		ipp_get_subscription_attributes(server_client_t *client);
static void		ipp_get_subscriptions(server_client_t *client);
static void		ipp_get_system_attributes(server_client_t *client);
static void		ipp_get_system_supported_values(server_client_t *client);
static void		ipp_hold_job(server_client_t *client);
static void		ipp_hold_new_jobs(server_client_t *client);
static void		ipp_identify_printer(server_client_t *client);
static void		ipp_install_resource(server_client_t *client);
static void		ipp_pause_all_printers(server_client_t *client);
static void		ipp_pause_printer(server_client_t *client);
static void		ipp_print_job(server_client_t *client);
static void		ipp_print_uri(server_client_t *client);
static void		ipp_register_output_device(server_client_t *client);
static void		ipp_release_held_new_jobs(server_client_t *client);
static void		ipp_release_job(server_client_t *client);
static void		ipp_renew_subscription(server_client_t *client);
static void		ipp_restart_printer(server_client_t *client);
static void		ipp_restart_system(server_client_t *client);
static void		ipp_resume_all_printers(server_client_t *client);
static void		ipp_resume_printer(server_client_t *client);
static void		ipp_send_document(server_client_t *client);
static void		ipp_send_resource_data(server_client_t *client);
static void		ipp_send_uri(server_client_t *client);
static void		ipp_set_document_attributes(server_client_t *client);
static void		ipp_set_job_attributes(server_client_t *client);
static void		ipp_set_printer_attributes(server_client_t *client);
static void		ipp_set_resource_attributes(server_client_t *client);
static void		ipp_set_system_attributes(server_client_t *client);
static void		ipp_shutdown_all_printers(server_client_t *client);
static void		ipp_shutdown_printer(server_client_t *client);
static void		ipp_startup_all_printers(server_client_t *client);
static void		ipp_startup_printer(server_client_t *client);
static void		ipp_update_active_jobs(server_client_t *client);
static void		ipp_update_document_status(server_client_t *client);
static void		ipp_update_job_status(server_client_t *client);
static void		ipp_update_output_device_attributes(server_client_t *client);
static void		ipp_validate_document(server_client_t *client);
static void		ipp_validate_job(server_client_t *client);
static void		respond_unsettable(server_client_t *client, ipp_attribute_t *attr);
static bool		valid_doc_attributes(server_client_t *client);
static bool		valid_filename(const char *filename);
static bool		valid_job_attributes(server_client_t *client);
static bool		valid_values(server_client_t *client, ipp_tag_t group_tag, ipp_attribute_t *supported, size_t num_values, server_value_t *values);
static float		wgs84_distance(const char *a, const char *b);


/*
 * Local globals...
 */

static server_value_t	job_values[] =		/* Value tags for job create/set attributes */
{
  { "chamber-humidity",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "chamber-temperature",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "copies",					IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "cover-back",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "cover-front",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "document-message",				IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_CREATEOP },
  { "document-metadata",			IPP_TAG_STRING, IPP_TAG_ZERO, VALUE_1SETOF | VALUE_CREATEOP },
  { "document-name",				IPP_TAG_NAME, IPP_TAG_ZERO, VALUE_NORMAL | VALUE_CREATEOP },
  { "finishings",				IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-col",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "imposition-template",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "insert-sheet",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-account-id",				IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-account-type",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-accounting-sheets",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-accounting-user-id",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-delay-output-until",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-delay-output-until-time",		IPP_TAG_DATE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-error-action",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-error-sheet",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-hold-until",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL | VALUE_CREATEOP },
  { "job-hold-until-time",			IPP_TAG_DATE, IPP_TAG_ZERO, VALUE_NORMAL | VALUE_CREATEOP },
  { "job-message-to-operator",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-name",					IPP_TAG_NAME, IPP_TAG_ZERO, VALUE_NORMAL | VALUE_CREATEOP },
  { "job-pages-per-set-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-phone-number",				IPP_TAG_URI, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-priority",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-recipient-name",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-retain-until",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-retain-until-time",			IPP_TAG_DATE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheet-message",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheets-col",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheets",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "materials-col",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-col",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "media",					IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "multiple-document-handling",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "multiple-object-handling",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "number-up",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "orientation-requested",			IPP_TAG_ENUM, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "output-bin",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "overrides",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "page-delivery",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "page-ranges",				IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_1SETOF },
  { "platform-temperature",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "presentation-direction-number-up",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-accuracy",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-base",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-color-mode",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-content-optimize",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-objects",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-quality",				IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-rendering-intent",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-scaling",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-supports",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-resolution",			IPP_TAG_RESOLUTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "proof-print",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "separator-sheets",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "sides",					IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-image-position",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-image-shift",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side1-image-shift",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side2-image-shift",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-image-position",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-image-shift",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side1-image-shift",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side2-image-shift",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL }
};
static server_value_t	printer_values[] =	/* Value tags for printer create/set attributes */
{
  { "chamber-humidity-default",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "chamber-humidity-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "chamber-temperature-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "chamber-temperature-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "coating-sides-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "coating-type-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "color-supported",				IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "copies-default",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "copies-supported",				IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "cover-back-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "cover-back-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "cover-front-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "cover-front-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "covering-name-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "document-creation-attributes-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "document-format-default",	 		IPP_TAG_MIMETYPE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "document-format-supported",		IPP_TAG_MIMETYPE, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishing-template-supported",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "finishings-default",			IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-ready",				IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-supported",			IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-col-database",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-col-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-col-ready",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "finishings-col-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "folding-direction-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "folding-offset-supported",			IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "folding-reference-edge-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "imposition-template-default",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "imposition-template-supported",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "insert-sheet-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "inseet-sheet-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-account-id-default",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-account-id-supported",			IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-account-type-default",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-account-type-supported",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "job-accounting-sheets-default",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-accounting-sheets-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-accounting-user-id-default",		IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-accounting-user-id-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-authorization-uri-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-constraints-supported",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-creation-attributes-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-delay-output-until-default",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-error-action-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-error-action-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-error-sheet-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-error-sheet-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-hold-until-default",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-message-to-operator-default",		IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-pages-per-set-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-password-encryption-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-password-length-supported",		IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-password-repertoire-configured",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-password-repertoire-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-password-supported",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-phone-number-default",			IPP_TAG_URI, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-phone-number-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-presets-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-priority-default",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-recipient-name-default",		IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "job-recipient-name-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-resolvers-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-retain-until-default",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-sheet-message-default",		IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheet-message-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheets-col-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "job-sheets-col-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "job-sheets-default",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "job-sheets-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "job-triggers-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "laminating-sides-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "laminating-type-supported",		IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "material-amount-units-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "material-diameter-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "material-nozzle-diameter-supported",	IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "material-purpose-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "material-rate-supported",			IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "material-rate-units-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "material-shell-thickness-supported",	IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "material-temperature-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "material-type-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "materials-col-database",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "materials-col-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "materials-col-ready",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "materials-col-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "max-materials-col-supported",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "max-stitching-locations-supported",	IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "media-bottom-margin-supported",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-col-database",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-col-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "media-col-ready",				IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-color-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "media-default",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "media-key-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "media-ready",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "media-supported",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "media-left-margin-supported",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-right-margin-supported",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-size-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-source-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "media-top-margin-supported",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_1SETOF },
  { "media-type-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "multiple-document-handling-default",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "multiple-document-jobs-supported",		IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "multiple-object-handling-default",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "multiple-operation-time-out-action",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "natural-language-configured",		IPP_TAG_LANGUAGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "notify-events-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "number-up-default",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "number-up-supported",			IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "orientation-requested-default",		IPP_TAG_ENUM, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "orientation-requested-supported",		IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_1SETOF },
  { "output-bin-default",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_NORMAL },
  { "output-bin-supported",			IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "overrides-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "page-delivery-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "page-delivery-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "page-ranges-supported",			IPP_TAG_BOOLEAN, IPP_TAG_ZERO, VALUE_NORMAL },
  { "pages-per-minute",				IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "pages-per-minute-color",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "pdl-override-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "platform-shape",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "platform-temperature-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "platform-temperature-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "presentation-direction-number-up-default",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "presentation-direction-number-up-supported", IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-accuracy-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-accuracy-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-base-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-base-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-color-mode-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-color-mode-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-content-optimize-default",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-content-optimize-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-objects-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-quality-default",			IPP_TAG_ENUM, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-rendering-intent-default",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-rendering-intent-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-scaling-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-scaling-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "print-supports-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "print-supports-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "printer-charge-info",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-charge-info-uri",			IPP_TAG_URI, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-contact-col",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_UNKNOWN, VALUE_NORMAL },
  { "printer-device-id",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-dns-sd-name",			IPP_TAG_NAME, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-geo-location",			IPP_TAG_URI, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-icc-profiles",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-info",				IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-kind",				IPP_TAG_KEYWORD, IPP_TAG_NAME, VALUE_1SETOF },
  { "printer-location",				IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-make-and-model",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-mandatory-job-attributes",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-name",				IPP_TAG_NAME, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-organization",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-organizational-unit",		IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-resolution-default",		IPP_TAG_RESOLUTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "printer-resolution-supported",		IPP_TAG_RESOLUTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "printer-volume-supported",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "proof-print-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "proof-print-suppported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "punching-hole-diameter-configured",	IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "punching-locations-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "punching-offset-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "punching-reference-edge-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION, IPP_TAG_ZERO, VALUE_1SETOF },
  { "pwg-raster-document-sheet-back",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "pwg-raster-document-type-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "pwg-safe-gcode-supported",			IPP_TAG_TEXT, IPP_TAG_ZERO, VALUE_1SETOF },
  { "separator-sheets-default",			IPP_TAG_BEGIN_COLLECTION, IPP_TAG_ZERO, VALUE_NORMAL },
  { "separator-sheets-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "sides-default",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "sides-supported",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "smi2699-auth-print-group",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "smi2699-auth-proxy-group",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "smi2699-device-command",			IPP_TAG_NAME, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "smi2699-device-format",			IPP_TAG_MIMETYPE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "smi2699-device-name",			IPP_TAG_NAME, IPP_TAG_ZERO, VALUE_NORMAL },
  { "smi2699-device-uri",			IPP_TAG_URI, IPP_TAG_NOVALUE, VALUE_NORMAL },
  { "smi2699-max-output-device",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "stitching-angle-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "stitching-locations-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "stitching-method-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "stitching-offset-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "stitching-reference-edge-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "trimming-offset-supported",		IPP_TAG_INTEGER, IPP_TAG_RANGE, VALUE_1SETOF },
  { "trimming-reference-edge-supported",	IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "trimming-type-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "trimming-when-supported",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "urf-supported",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "faxout-supported",				IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "x-image-position-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-image-position-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "x-image-shift-default",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-image-shift-supported",			IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side1-image-shift-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side1-image-shift-supported",		IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side2-image-shift-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "x-side2-image-shift-supported",		IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-image-position-default",			IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-image-position-supported",		IPP_TAG_KEYWORD, IPP_TAG_ZERO, VALUE_1SETOF },
  { "y-image-shift-default",			IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-image-shift-supported",			IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side1-image-shift-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side1-image-shift-supported",		IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side2-image-shift-default",		IPP_TAG_INTEGER, IPP_TAG_ZERO, VALUE_NORMAL },
  { "y-side2-image-shift-supported",		IPP_TAG_RANGE, IPP_TAG_ZERO, VALUE_NORMAL }
};


/*
 * 'serverCopyAttributes()' - Copy attributes from one request to another.
 */

void
serverCopyAttributes(
    ipp_t        *to,			/* I - Destination request */
    ipp_t        *from,			/* I - Source request */
    cups_array_t *ra,			/* I - Requested attributes */
    cups_array_t *pa,			/* I - Private attributes */
    ipp_tag_t    group_tag,		/* I - Group to copy */
    int          quickcopy)		/* I - Do a quick copy? */
{
  server_filter_t	filter;		/* Filter data */


  filter.ra        = ra;
  filter.pa        = pa;
  filter.group_tag = group_tag;

  ippCopyAttributes(to, from, quickcopy, (ipp_copycb_t)filter_cb, &filter);
}


/*
 * 'apply_template_attributes()' - Apply attributes from a template resource.
 */

static bool				/* O - `true` if valid, `false` if not */
apply_template_attributes(
    ipp_t             *to,		/* I - Destination attributes */
    ipp_tag_t         to_group_tag,	/* I - Destination group */
    server_resource_t *template,	/* I - Template resource */
    ipp_attribute_t   *supported,	/* I - "xxx-supported" attribute, if any */
    size_t            num_values,	/* I - Number of value definitions */
    server_value_t    *values)		/* I - Value definitions */
{
  int			fd;		/* Resource file descriptor */
  ipp_t			*from;		/* Resource attributes */
  ipp_attribute_t	*fromattr,	/* Source attribute */
			*toattr;	/* Destination attribute */
  const char		*name;		/* Source attribute name */
  ipp_tag_t		value_tag;	/* Source value tag */
  size_t		i;		/* Looping var */
  server_value_t	*value;		/* Current value definition */


 /*
  * Load the resource attributes...
  */

  if ((fd = open(template->filename, O_RDONLY | O_BINARY)) < 0)
  {
    serverLog(SERVER_LOGLEVEL_ERROR, "Unable to open resource %d file \"%s\": %s", template->id, template->filename, strerror(errno));
    return (false);
  }

  from = ippNew();

  if (ippReadFile(fd, from) != IPP_STATE_DATA)
  {
    serverLog(SERVER_LOGLEVEL_ERROR, "Unable to read resource %d file \"%s\": %s", template->id, template->filename, cupsLastErrorString());
    close(fd);
    ippDelete(from);
    return (false);
  }

  close(fd);

 /*
  * Loop through the attributes, validate, and copy as needed...
  */

  for (fromattr = ippFirstAttribute(from); fromattr; fromattr = ippNextAttribute(from))
  {
    name      = ippGetName(fromattr);
    value_tag = ippGetValueTag(fromattr);

    if (!name || (supported && !ippContainsString(supported, name)) || ippFindAttribute(to, name, IPP_TAG_ZERO))
      continue;

    for (i = num_values, value = values; i > 0; i --, value ++)
    {
      if (!strcmp(name, value->name) && (value_tag == value->value_tag || value_tag == value->alt_tag) && (ippGetCount(fromattr) == 1 || (value->flags & VALUE_1SETOF)))
      {
	toattr = ippCopyAttribute(to, fromattr, 0);
	ippSetGroupTag(to, &toattr, to_group_tag);
        break;
      }
    }
  }

 /*
  * Clean up...
  */

  ippDelete(from);

  return (true);
}


/*
 * 'copy_doc_attrs()' - Copy document attributes to the response.
 */

static void
copy_doc_attributes(
    server_client_t *client,		/* I - Client */
    server_job_t    *job,		/* I - Job */
    cups_array_t    *ra,		/* I - requested-attributes */
    cups_array_t    *pa)		/* I - Private attributes */
{
  const char		*name;		/* Attribute name */
  ipp_attribute_t	*srcattr;	/* Source attribute */


 /*
  * Synthesize/copy the following Document Description/Status attributes:
  *
  *   compression ("none")
  *   date-time-at-xxx
  *   document-access-errors
  *   document-job-id (from job-id)
  *   document-job-uri (from job-uri)
  *   document-printer-uri (from job-printer-uri)
  *   document-metadata
  *   document-number (1)
  *   document-name
  *   document-uri
  *   document-uuid (from job-uuid)
  *   impressions (from job-impressions)
  *   impressions-col (from job-impressions-col)
  *   impressions-completed (from job-impressions-completed)
  *   impressions-completed-col (from job-impressions-completed-col)
  *   k-octets (from job-k-octets)
  *   last-document (true)
  *   media-sheets (from job-media-sheets)
  *   media-sheets-col (from job-media-sheets-col)
  *   media-sheets-completed (from job-media-sheets-completed)
  *   media-sheets-completed-col (from job-media-sheets-completed-col)
  *   pages (from job-pages)
  *   pages-col (from job-pages-col)
  *   pages-completed (from job-pages-completed)
  *   pages-completed-col (from job-pages-completed-col)
  *   time-at-xxx
  */

  serverCopyAttributes(client->response, job->doc_attrs, ra, pa, IPP_TAG_DOCUMENT, 0);

  for (srcattr = ippFirstAttribute(job->attrs); srcattr; srcattr = ippNextAttribute(job->attrs))
  {
    if (ippGetGroupTag(srcattr) != IPP_TAG_JOB || (name = ippGetName(srcattr)) == NULL)
      continue;

    if ((!strncmp(name, "job-impressions", 15) || !strncmp(name, "job-k-octets", 12) || !strncmp(name, "job-media-sheets", 16) || !strncmp(name, "job-pages", 9)) && check_attribute(name + 4, ra, pa))
    {
      name += 4;

      if (strstr(name, "-col"))
        ippAddCollection(client->response, IPP_TAG_DOCUMENT, name, ippGetCollection(srcattr, 0));
      else
        ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, name, ippGetInteger(srcattr, 0));
    }
    else if (!strcmp(name, "document-uri") && check_attribute("document-uri", ra, pa))
      ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-uri", NULL, ippGetString(srcattr, 0, NULL));
    else if (!strcmp(name, "job-printer-uri") && check_attribute("document-printer-uri", ra, pa))
      ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-printer-uri", NULL, ippGetString(srcattr, 0, NULL));
    else if (!strcmp(name, "job-uri") && check_attribute("document-job-uri", ra, pa))
      ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-job-uri", NULL, ippGetString(srcattr, 0, NULL));
    else if (!strcmp(name, "job-uuid") && check_attribute("document-uuid", ra, pa))
      ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_URI, "document-uuid", NULL, ippGetString(srcattr, 0, NULL));
  }

  if (check_attribute("date-time-at-completed", ra, pa))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_DOCUMENT, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (check_attribute("date-time-at-created", ra, pa))
    ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-created", ippTimeToDate(job->created));

  if (check_attribute("date-time-at-processing", ra, pa))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_DOCUMENT, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_DOCUMENT, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (check_attribute("document-format", ra, pa))
    ippAddString(client->response, IPP_TAG_DOCUMENT, IPP_TAG_MIMETYPE, "document-format", NULL, job->format);

  if (check_attribute("document-job-id", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "document-job-id", job->id);

  if (check_attribute("document-number", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "document-number", 1);

  if (check_attribute("document-state", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_ENUM, "document-state", (int)job->state);

  if (check_attribute("document-state-reasons", ra, pa))
    serverCopyJobStateReasons(client->response, IPP_TAG_DOCUMENT, job);

  if (check_attribute("impressions", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "impressions", job->impressions);

  if (check_attribute("impressions-completed", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "impressions-completed", job->impcompleted);

  if (check_attribute("last-document", ra, pa))
    ippAddBoolean(client->response, IPP_TAG_DOCUMENT, "last-document", 1);

  if (check_attribute("time-at-completed", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (check_attribute("time-at-created", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, IPP_TAG_INTEGER, "time-at-created", (int)(job->created - client->printer->start_time));

  if (check_attribute("time-at-processing", ra, pa))
    ippAddInteger(client->response, IPP_TAG_DOCUMENT, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
}


/*
 * 'copy_document_uri()' - Make a copy of the referenced document for printing.
 */

static int				/* O - 1 on success, 0 on failure */
copy_document_uri(
    server_client_t *client,		/* I - Client connection */
    server_job_t    *job,		/* I - Print job */
    const char      *uri)		/* I - Document URI */
{
  ipp_attribute_t	*attr;		/* document-format-detected attribute */
  char			redirect[1024],	/* Redirect URI */
			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  const  char		*content_type;	/* Content-Type from server */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  http_encryption_t	encryption;	/* Encryption to use, if any */
  http_t		*http;		/* Connection for http/https URIs */
  http_status_t		status;		/* Access status for http/https URIs */
  char			filename[1024],	/* Filename buffer */
			buffer[16384];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */


 /*
  * Pull the URI apart...  We already know it will work here since we validated
  * the URI in get_document_uri().
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource));

 /*
  * "file" URIs refer to local files...
  */

  if (!strcmp(scheme, "file"))
  {
    int infile;			/* Input file for local file URIs */

    if ((infile = open(resource, O_RDONLY | O_NOFOLLOW | O_BINARY)) < 0)
    {
      job->state = IPP_JSTATE_ABORTED;

      serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to access URI: %s", strerror(errno));
      return (0);
    }

    if (!strcmp(job->format, "application/octet-stream"))
    {
      memset(buffer, 0, 8);

      if (read(infile, buffer, 8) > 0 && (content_type = detect_format((unsigned char *)buffer)) != NULL)
      {
	cupsRWLockWrite(&job->rwlock);

	attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, content_type);

	cupsRWUnlock(&job->rwlock);

	job->format = ippGetString(attr, 0, NULL);
      }

      lseek(infile, 0, SEEK_SET);
    }

   /*
    * Create a file for the request data...
    */

    serverCreateJobFilename(job, job->format, filename, sizeof(filename));

    if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0)
    {
      close(infile);

      job->state = IPP_JSTATE_ABORTED;

      serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));
      return (0);
    }

   /*
    * Copy the file...
    */

    do
    {
      if ((bytes = read(infile, buffer, sizeof(buffer))) < 0 && (errno == EAGAIN || errno == EINTR))
      {
       /*
        * Force a retry of the read...
        */

        bytes = 1;
      }
      else if (bytes > 0 && write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	close(infile);

	serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));
	return (0);
      }
    }
    while (bytes > 0);

    close(infile);

    goto finalize_copy;
  }

 /*
  * Loop until we find the network resource...
  */

  for (;;)
  {
    serverLogJob(SERVER_LOGLEVEL_DEBUG, job, "GET %s", uri);

    if (port == 443 || !strcmp(scheme, "https"))
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    if ((http = httpConnect(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to connect to %s: %s", hostname, cupsLastErrorString());
      job->state = IPP_JSTATE_ABORTED;

      return (0);
    }

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    if (httpGet(http, resource))
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to GET URI: %s", strerror(errno));

      job->state = IPP_JSTATE_ABORTED;

      httpClose(http);
      return (0);
    }

    while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

    serverLogJob(SERVER_LOGLEVEL_DEBUG, job, "GET returned status %d", status);

    if (status == HTTP_STATUS_MOVED_PERMANENTLY || status == HTTP_STATUS_FOUND || status == HTTP_STATUS_SEE_OTHER)
    {
     /*
      * Follow redirection...
      */

      cupsCopyString(redirect, httpGetField(http, HTTP_FIELD_LOCATION), sizeof(redirect));
      httpClose(http);

      uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, redirect, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource));
      if (uri_status < HTTP_URI_STATUS_OK)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Redirected to bad URI \"%s\": %s", redirect, httpURIStatusString(uri_status));

	job->state = IPP_JSTATE_ABORTED;

	return (0);
      }

      if (strcmp(scheme, "http") && strcmp(scheme, "https"))
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Redirected to unsupported URI scheme \"%s\".", scheme);

	job->state = IPP_JSTATE_ABORTED;

	return (0);
      }

      uri = redirect;

      continue;
    }
    else if (status != HTTP_STATUS_OK)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to GET URI: %s", httpStatusString(status));

      job->state = IPP_JSTATE_ABORTED;

      httpClose(http);

      return (0);
    }

   /*
    * If we get this far, get the document from the URI...
    */

    content_type = httpGetField(http, HTTP_FIELD_CONTENT_TYPE);
    if (*content_type)
    {
      serverLogJob(SERVER_LOGLEVEL_INFO, job, "URI Content-Type=\"%s\"", content_type);

      cupsRWLockWrite(&job->rwlock);

      attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, content_type);

      cupsRWUnlock(&job->rwlock);

      job->format = ippGetString(attr, 0, NULL);
    }
    else
      content_type = job->format;

    serverCreateJobFilename(job, content_type, filename, sizeof(filename));

    if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0)
    {
      job->state = IPP_JSTATE_ABORTED;

      httpClose(http);

      serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create print file: %s", strerror(errno));

      return (0);
    }

    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
    {
      if (write(job->fd, buffer, (size_t)bytes) < bytes)
      {
	int error = errno;		/* Write error */

	job->state = IPP_JSTATE_ABORTED;

	close(job->fd);
	job->fd = -1;

	unlink(filename);
	httpClose(http);

	serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(error));
	return (0);
      }
    }

    httpClose(http);
    break;
  }

 /*
  * Finalize copy...
  */

  finalize_copy:

  if (close(job->fd))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write print file: %s", strerror(errno));

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    return (0);
  }

  job->fd       = -1;
  job->filename = strdup(filename);

  return (1);
}


/*
 * 'copy_job_attrs()' - Copy job attributes to the response.
 */

static void
copy_job_attributes(
    server_client_t *client,		/* I - Client */
    server_job_t    *job,		/* I - Job */
    cups_array_t    *ra,		/* I - requested-attributes */
    cups_array_t    *pa)		/* I - Private attributes */
{
  serverCopyAttributes(client->response, job->attrs, ra, pa, IPP_TAG_JOB, 0);

  if (check_attribute("date-time-at-completed", ra, pa))
  {
    if (job->completed)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-completed", ippTimeToDate(job->completed));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-completed");
  }

  if (check_attribute("date-time-at-processing", ra, pa))
  {
    if (job->processing)
      ippAddDate(client->response, IPP_TAG_JOB, "date-time-at-processing", ippTimeToDate(job->processing));
    else
      ippAddOutOfBand(client->response, IPP_TAG_JOB, IPP_TAG_NOVALUE, "date-time-at-processing");
  }

  if (check_attribute("job-impressions", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions", job->impressions);

  if (check_attribute("job-impressions-completed", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-impressions-completed", job->impcompleted);

  if (check_attribute("job-printer-up-time", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (check_attribute("job-state", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state", (int)job->state);

  if (check_attribute("job-state-message", ra, pa))
  {
    if (job->dev_state_message)
    {
      ippAddString(client->response, IPP_TAG_JOB, IPP_TAG_TEXT, "job-state-message", NULL, job->dev_state_message);
    }
    else
    {
      const char *message = "";		/* Message string */

      switch (job->state)
      {
	case IPP_JSTATE_PENDING :
	    message = "Job pending.";
	    break;

	case IPP_JSTATE_HELD :
	    if (job->state_reasons & SERVER_JREASON_JOB_INCOMING)
	      message = "Job incoming.";
	    else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO))
	      message = "Job held.";
	    else
	      message = "Job created.";
	    break;

	case IPP_JSTATE_PROCESSING :
	    if (job->state_reasons & SERVER_JREASON_PROCESSING_TO_STOP_POINT)
	    {
	      if (job->cancel)
		message = "Cancel in progress.";
	      else
	        message = "Abort in progress.";
	    }
	    else
	      message = "Job printing.";
	    break;

	case IPP_JSTATE_STOPPED :
	    message = "Job stopped.";
	    break;

	case IPP_JSTATE_CANCELED :
	    message = "Job canceled.";
	    break;

	case IPP_JSTATE_ABORTED :
	    message = "Job aborted.";
	    break;

	case IPP_JSTATE_COMPLETED :
	    message = "Job completed.";
	    break;
      }

      ippAddString(client->response, IPP_TAG_JOB, IPP_CONST_TAG(IPP_TAG_TEXT), "job-state-message", NULL, message);
    }
  }

  if (check_attribute("job-state-reasons", ra, pa))
    serverCopyJobStateReasons(client->response, IPP_TAG_JOB, job);

  if (check_attribute("number-of-documents", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "number-of-documents", job->filename ? 1 : 0);

  if (check_attribute("time-at-completed", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, job->completed ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-completed", (int)(job->completed - client->printer->start_time));

  if (check_attribute("time-at-processing", ra, pa))
    ippAddInteger(client->response, IPP_TAG_JOB, job->processing ? IPP_TAG_INTEGER : IPP_TAG_NOVALUE, "time-at-processing", (int)(job->processing - client->printer->start_time));
}


/*
 * 'copy_printer_attributes()' - Copy all printer attributes.
 */

static void
copy_printer_attributes(
    server_client_t  *client,		/* I - Client */
    server_printer_t *printer,		/* I - Printer */
    cups_array_t     *ra)		/* I - Requested attributes */
{
  serverCopyAttributes(client->response, printer->pinfo.attrs, ra, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO);
  serverCopyAttributes(client->response, printer->dev_attrs, ra, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO);
  serverCopyAttributes(client->response, PrivacyAttributes, ra, NULL, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);

  if (!ra || cupsArrayFind(ra, "printer-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-config-change-date-time", ippTimeToDate(printer->config_time));

  if (!ra || cupsArrayFind(ra, "printer-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-config-change-time", (int)(printer->config_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(client->response, IPP_TAG_PRINTER, "printer-current-time", ippTimeToDate(time(NULL)));

  if (!ra || cupsArrayFind(ra, "printer-dns-sd-name"))
  {
    if (printer->dns_sd_name)
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_NAME, "printer-dns-sd-name", NULL, printer->dns_sd_name);
    else
      ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_NOVALUE, "printer-dns-sd-name");
  }

  copy_printer_state(client->response, printer, ra);

  if (printer->num_resources && (!ra || cupsArrayFind(ra, "printer-resource-ids")))
    ippAddIntegers(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-resource-ids", printer->num_resources, printer->resources);

  if (printer->pinfo.strings && (!ra || cupsArrayFind(ra, "printer-strings-uri")))
  {
   /*
    * See if we have a localization that matches the request language.
    */

    ipp_attribute_t	*attr;		/* attributes-natural-language attribute */
    char		lang[32];	/* Copy of language string */
    server_lang_t	key, *match;	/* Localization key and match */

    ippFirstAttribute(client->request);
    attr = ippNextAttribute(client->request);
    cupsCopyString(lang, ippGetString(attr, 0, NULL), sizeof(lang));
    key.lang = lang;
    if ((match = cupsArrayFind(printer->pinfo.strings, &key)) == NULL && lang[2])
    {
     /*
      * Try base language...
      */

      lang[2] = '\0';
      match = cupsArrayFind(printer->pinfo.strings, &key);
    }

    if (match)
    {
      char		uri[1024];	/* printer-strings-uri value */
      server_listener_t	*lis = cupsArrayGetFirst(Listeners);
					/* Default listener */
      const char	*scheme = "http";
					/* URL scheme */

      if (Encryption != HTTP_ENCRYPTION_NEVER)
        scheme = "https";

      httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), scheme, NULL, lis->host, lis->port, match->resource->resource);
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-strings-uri", NULL, uri);
    }
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    ippAddInteger(client->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "queued-job-count", (int)cupsArrayGetCount(printer->active_jobs));
}


/*
 * 'copy_printer_state()' - Copy printer state attributes.
 */

static void
copy_printer_state(
    ipp_t            *ipp,		/* I - Destination IPP message */
    server_printer_t *printer,		/* I - Printer */
    cups_array_t     *ra)		/* I - Requested attributes */
{
  if (!ra || cupsArrayFind(ra, "printer-is-accepting-jobs"))
    ippAddBoolean(ipp, IPP_TAG_PRINTER, "printer-is-accepting-jobs", printer->is_accepting);

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", printer->state > printer->dev_state ? (int)printer->state : (int)printer->dev_state);

  if (!ra || cupsArrayFind(ra, "printer-state-change-date-time"))
    ippAddDate(ipp, IPP_TAG_PRINTER, "printer-state-change-date-time", ippTimeToDate(printer->state_time));

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "printer-state-change-time", (int)(printer->state_time - printer->start_time));

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Printing.", "Stopped." };

    if (printer->state > printer->dev_state)
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->state - IPP_PSTATE_IDLE]);
    else
      ippAddString(ipp, IPP_TAG_PRINTER, IPP_CONST_TAG(IPP_TAG_TEXT), "printer-state-message", NULL, messages[printer->dev_state - IPP_PSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
    serverCopyPrinterStateReasons(ipp, IPP_TAG_PRINTER, printer);
}


/*
 * 'copy_resource_attributes()' - Copy the attributes for a resource.
 */

static void
copy_resource_attributes(
    server_client_t   *client,		/* I - Client */
    server_resource_t *resource,	/* I - Resource */
    cups_array_t      *ra)		/* I - requested-attributes */
{
  serverCopyAttributes(client->response, resource->attrs, ra, NULL, IPP_TAG_RESOURCE, 0);

  /* resource-state */
  if (!ra || cupsArrayFind(ra, "resource-state"))
  {
    ippAddInteger(client->response, IPP_TAG_RESOURCE, IPP_TAG_ENUM, "resource-state", (int)resource->state);
  }

  /* resource-state-reasons */
  if (!ra || cupsArrayFind(ra, "resource-state-reasons"))
  {
    ippAddString(client->response, IPP_TAG_RESOURCE, IPP_TAG_KEYWORD, "resource-state-reasons", NULL, resource->fd >= 0 ? "resource-incoming" : resource->cancel ? "cancel-requested" : "none");
  }

  /* resource-use-count */
  if (!ra || cupsArrayFind(ra, "resource-use-count"))
  {
    ippAddInteger(client->response, IPP_TAG_RESOURCE, IPP_TAG_INTEGER, "resource-use-count", resource->use);
  }
}


/*
 * 'copy_sub_attrs()' - Copy job attributes to the response.
 */

static void
copy_subscription_attributes(
    server_client_t       *client,	/* I - Client */
    server_subscription_t *sub,		/* I - Subscription */
    cups_array_t          *ra,		/* I - requested-attributes */
    cups_array_t          *pa)		/* I - Private attributes */
{
  serverCopyAttributes(client->response, sub->attrs, ra, pa, IPP_TAG_SUBSCRIPTION, 0);

  if (!sub->job && check_attribute("notify-lease-expiration-time", ra, pa))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-expiration-time", (int)(sub->expire - client->printer->start_time));

  if (!sub->job && check_attribute("notify-printer-up-time", ra, pa))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-printer-up-time", (int)(time(NULL) - client->printer->start_time));

  if (check_attribute("notify-sequence-number", ra, pa))
    ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-sequence-number", sub->last_sequence);
}


/*
 * 'copy_system_state()' - Copy the current system state.
 */

static void
copy_system_state(ipp_t        *ipp,	/* I - IPP message */
                  cups_array_t *ra)	/* I - Requested attributes */
{
  ipp_pstate_t		state = IPP_PSTATE_STOPPED;
					/* system-state */
  server_preason_t	state_reasons = SERVER_PREASON_NONE;
					/* system-state-reasons */
  time_t		state_time = 0;	/* system-state-change-[date-]time */
  server_printer_t	*printer;	/* Current printer */


  if (!ra || cupsArrayFind(ra, "system-state") || cupsArrayFind(ra, "system-state-change-date-time") || cupsArrayFind(ra, "system-state-change-time") || cupsArrayFind(ra, "system-state-message") || cupsArrayFind(ra, "system-state-reasons"))
  {
    cupsRWLockRead(&PrintersRWLock);

    for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    {
      if (printer->state == IPP_PSTATE_PROCESSING)
        state = IPP_PSTATE_PROCESSING;
      else if (printer->state == IPP_PSTATE_IDLE && state == IPP_PSTATE_STOPPED)
        state = IPP_PSTATE_IDLE;

      state_reasons |= printer->state_reasons | printer->dev_reasons;

      if (printer->state_time > state_time)
        state_time = printer->state_time;
    }

    cupsRWUnlock(&PrintersRWLock);
  }

  if (!ra || cupsArrayFind(ra, "system-state"))
    ippAddInteger(ipp, IPP_TAG_SYSTEM, IPP_TAG_ENUM, "system-state", (int)state);

  if (!ra || cupsArrayFind(ra, "system-state-change-date-time"))
    ippAddDate(ipp, IPP_TAG_SYSTEM, "system-state-change-date-time", ippTimeToDate(state_time));

  if (!ra || cupsArrayFind(ra, "system-state-change-time"))
    ippAddInteger(ipp, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-state-change-time", (int)(state_time - SystemStartTime));

  if (!ra || cupsArrayFind(ra, "system-state-message"))
  {
    if (state == IPP_PSTATE_IDLE)
      ippAddString(ipp, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_TEXT), "system-state-message", NULL, "Idle.");
    else if (state == IPP_PSTATE_PROCESSING)
      ippAddString(ipp, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_TEXT), "system-state-message", NULL, "Printing.");
    else
      ippAddString(ipp, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_TEXT), "system-state-message", NULL, "Stopped.");
  }

  if (!ra || cupsArrayFind(ra, "system-state-reasons"))
  {
    if (state_reasons == SERVER_PREASON_NONE)
    {
      ippAddString(ipp, IPP_TAG_SYSTEM, IPP_TAG_KEYWORD, "system-state-reasons", NULL, "none");
    }
    else
    {
      size_t		i,		/* Looping var */
			num_reasons = 0;/* Number of reasons */
      server_preason_t	reason;		/* Current reason */
      const char	*reasons[32];	/* Reason strings */

      for (i = 0, reason = 1; i < (sizeof(server_preasons) / sizeof(server_preasons[0])); i ++, reason <<= 1)
      {
	if (state_reasons & reason)
	  reasons[num_reasons ++] = server_preasons[i];
      }

      ippAddStrings(ipp, IPP_TAG_SYSTEM, IPP_CONST_TAG(IPP_TAG_KEYWORD), "system-state-reasons", num_reasons, NULL, reasons);
    }
  }
}


/*
 * 'detect_format()' - Auto-detect the file format from the initial header
 *                     bytes.
 */

static const char *			/* O - MIME type or `NULL` if none */
detect_format(
    const unsigned char *header)	/* I - First 8 bytes of file */
{
  if (!memcmp(header, "%PDF", 4))
    return ("application/pdf");
  else if (!memcmp(header, "%!", 2))
    return ("application/postscript");
  else if (!memcmp(header, "\377\330\377", 3) && header[3] >= 0xe0 && header[3] <= 0xef)
    return ("image/jpeg");
  else if (!memcmp(header, "\211PNG", 4))
    return ("image/png");
  else if (!memcmp(header, "RAS2", 4))
    return ("image/pwg-raster");
  else if (!memcmp(header, "UNIRAST", 8))
    return ("image/urf");
  else
    return (NULL);
}


/*
 * 'filter_cb()' - Filter printer attributes based on the requested array.
 */

static int				/* O - 1 to copy, 0 to ignore */
filter_cb(server_filter_t   *filter,	/* I - Filter parameters */
          ipp_t           *dst,		/* I - Destination (unused) */
	  ipp_attribute_t *attr)	/* I - Source attribute */
{
 /*
  * Filter attributes as needed...
  */

#ifndef _WIN32 /* Avoid MS compiler bug */
  (void)dst;
#endif /* !_WIN32 */

  ipp_tag_t group = ippGetGroupTag(attr);
  const char *name = ippGetName(attr);

  if ((filter->group_tag != IPP_TAG_ZERO && group != filter->group_tag && group != IPP_TAG_ZERO) || !name || (!strcmp(name, "media-col-database") && !cupsArrayFind(filter->ra, (void *)name)))
    return (0);

  if (filter->pa && cupsArrayFind(filter->pa, (void *)name))
    return (0);

  return (!filter->ra || cupsArrayFind(filter->ra, (void *)name) != NULL);
}


/*
 * 'get_document_uri()' - Get and validate the document-uri for printing.
 */

static const char *			/* O - Document URI or `NULL` on error */
get_document_uri(
    server_client_t *client)		/* I - Client connection */
{
  ipp_attribute_t	*uri;		/* document-uri */
  char			scheme[256],	/* URI scheme */
			userpass[256],	/* Username and password info */
			hostname[256],	/* Hostname */
			resource[1024];	/* Resource path */
  int			port;		/* Port number */
  http_uri_status_t	uri_status;	/* URI decode status */
  struct stat		fileinfo;	/* File information */


  if ((uri = ippFindAttribute(client->request, "document-uri", IPP_TAG_URI)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing document-uri.");
    return (NULL);
  }

  if (ippGetCount(uri) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Too many document-uri values.");
    serverRespondUnsupported(client, uri);
    return (NULL);
  }

  uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, resource, sizeof(resource));
  if (uri_status < HTTP_URI_STATUS_OK)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad document-uri: %s", httpURIStatusString(uri_status));
    serverRespondUnsupported(client, uri);
    return (NULL);
  }

  if (strcmp(scheme, "file") && strcmp(scheme, "https") && strcmp(scheme, "http"))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_URI_SCHEME, "URI scheme \"%s\" not supported.", scheme);
    serverRespondUnsupported(client, uri);
    return (NULL);
  }

  if (!strcmp(scheme, "file") && (!valid_filename(resource) || access(resource, R_OK) || lstat(resource, &fileinfo) || !S_ISREG(fileinfo.st_mode)))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_DOCUMENT_ACCESS, "Unable to access URI: %s", strerror(errno));
    serverRespondUnsupported(client, uri);
    return (NULL);
  }

 /*
  * If we get this far the URI is valid.  We'll check for accessibility in
  * copy_document_uri()...
  */

  return (ippGetString(uri, 0, NULL));
}


/*
 * 'ipp_acknowledge_document()' - Acknowledge receipt of a document.
 */

static void
ipp_acknowledge_document(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_acknowledge_identify_printer()' - Acknowledge an identify command.
 */

static void
ipp_acknowledge_identify_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockWrite(&client->printer->rwlock);

  if (client->printer->identify_actions)
  {
    static const char * const identify_actions[] =
    {
      "display",
      "sound"
    };

    serverRespondIPP(client, IPP_STATUS_OK, NULL);

    if (client->printer->identify_actions == SERVER_IDENTIFY_DISPLAY)
      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "identify-actions", NULL, "display");
    else if (client->printer->identify_actions == SERVER_IDENTIFY_SOUND)
      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "identify-actions", NULL, "sound");
    else
      ippAddStrings(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "identify-actions", 2, NULL, identify_actions);
    client->printer->identify_actions = SERVER_IDENTIFY_NONE;

    if (client->printer->identify_message)
    {
      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT, "message", NULL, client->printer->identify_message);
      free(client->printer->identify_message);
      client->printer->identify_message = NULL;
    }

    client->printer->state_reasons &= (unsigned)~SERVER_PREASON_IDENTIFY_PRINTER_REQUESTED;

    serverAddEventNoLock(client->printer, NULL, NULL, SERVER_EVENT_PRINTER_STATE_CHANGED, "Identify-Printer request received.");
  }
  else
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "No pending Identify-Printer request.");

  cupsRWUnlock(&client->printer->rwlock);
}


/*
 * 'ipp_acknowledge_job()' - Acknowledge receipt of a job.
 */

static void
ipp_acknowledge_job(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (job->dev_uuid && strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Job not assigned to device.");
    return;
  }

  if (!(job->state_reasons & SERVER_JREASON_JOB_FETCHABLE))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FETCHABLE, "Job not fetchable.");
    return;
  }

  if (!job->dev_uuid)
    job->dev_uuid = strdup(device->uuid);

  job->state_reasons &= (server_jreason_t)~SERVER_JREASON_JOB_FETCHABLE;

  serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_STATE_CHANGED, "Job acknowledged.");

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_allocate_printer_resources()' - Allocate resources for a printer.
 */

static void
ipp_allocate_printer_resources(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer = client->printer;
					/* Printer */
  ipp_attribute_t	*resource_ids;	/* resource-ids attribute */
  size_t		i,		/* Looping var */
			count;		/* Number of values */
  int			resource_id;	/* Current resource ID */
  server_resource_t	*resource;	/* Current resource */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((resource_ids = ippFindAttribute(client->request, "resource-ids", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'resource-ids' attribute.");
    return;
  }
  else if (ippGetGroupTag(resource_ids) != IPP_TAG_OPERATION)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The 'resource-ids' attribute is in the wrong group.");
    return;
  }
  else if (ippGetValueTag(resource_ids) != IPP_TAG_INTEGER)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "The 'resource-ids' attribute is the wrong type.");
    serverRespondUnsupported(client, resource_ids);
    return;
  }

  cupsRWLockWrite(&printer->rwlock);

  for (i = 0, count = ippGetCount(resource_ids); i < count; i ++)
  {
    resource_id = ippGetInteger(resource_ids, i);
    resource    = serverFindResourceById(resource_id);

    if (!resource)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d does not exist.", resource_id);
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&printer->rwlock);
      return;
    }
    else if (resource->state != IPP_RSTATE_INSTALLED)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is not installed.", resource_id);
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&printer->rwlock);
      return;
    }
    else if (strncmp(resource->type, "static-", 7))
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d of type '%s' cannot be allocated.", resource_id, resource->type);
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&printer->rwlock);
      return;
    }
  }

 /*
  * Allocate resources...
  */

  for (i = 0, count = ippGetCount(resource_ids); i < count; i ++)
  {
    resource_id = ippGetInteger(resource_ids, i);
    resource    = serverFindResourceById(resource_id);

    serverAllocatePrinterResource(printer, resource);
  }

  cupsRWUnlock(&printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_cancel_current_job()' - Cancel the current job.
 */

static void
ipp_cancel_current_job(
    server_client_t *client)		/* I - Client */
{
  server_job_t		*job;		/* Job information */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the current job, if any...
  */

  cupsRWLockWrite(&(client->printer->rwlock));

  if ((job = client->printer->processing_job) == NULL)
  {
    cupsRWUnlock(&client->printer->rwlock);
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No job being processed.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    cupsRWUnlock(&client->printer->rwlock);
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
  {
    job->cancel = 1;

    if (job->state == IPP_JSTATE_PROCESSING)
      serverStopJob(job);
  }
  else
  {
    job->state     = IPP_JSTATE_CANCELED;
    job->completed = time(NULL);
  }

  cupsRWUnlock(&(client->printer->rwlock));

  serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_COMPLETED, NULL);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_cancel_document()' - Cancel a document in a job.
 */

static void
ipp_cancel_document(
    server_client_t *client)		/* I - Client */
{
  server_job_t		*job;		/* Job information */
  ipp_attribute_t	*attr;		/* Document number attribute */
  int			doc_number;	/* Document number value */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad 'document-number' attribute in request." : "Missing 'document-number' attribute in request.");
    return;
  }
  else if ((doc_number = ippGetInteger(attr, 0)) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Document #%d does not exist.", doc_number);
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already canceled - can\'t cancel.", doc_number);
        break;

    case IPP_JSTATE_ABORTED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already aborted - can\'t cancel.", doc_number);
        break;

    case IPP_JSTATE_COMPLETED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Document #%d is already completed - can\'t cancel.", doc_number);
        break;

    default :
       /*
        * Cancel the job...
	*/

	cupsRWLockWrite(&(client->printer->rwlock));

	if (job->state == IPP_JSTATE_PROCESSING ||
	    (job->state == IPP_JSTATE_HELD && job->fd >= 0))
        {
          job->cancel = 1;

          if (job->state == IPP_JSTATE_PROCESSING)
	    serverStopJob(job);
	}
	else
	{
	  job->state     = IPP_JSTATE_CANCELED;
	  job->completed = time(NULL);
	}

	cupsRWUnlock(&(client->printer->rwlock));

        serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_COMPLETED, NULL);

	serverRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }}


/*
 * 'ipp_cancel_job()' - Cancel a job.
 */

static void
ipp_cancel_job(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* Job information */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already canceled - can't cancel.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already aborted - can't cancel.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d is already completed - can't cancel.", job->id);
        break;

    default :
       /*
        * Cancel the job...
	*/

	cupsRWLockWrite(&(client->printer->rwlock));

	if (job->state == IPP_JSTATE_PROCESSING ||
	    (job->state == IPP_JSTATE_HELD && job->fd >= 0))
        {
          job->cancel = 1;

          if (job->state == IPP_JSTATE_PROCESSING)
	    serverStopJob(job);
	}
	else
	{
	  job->state     = IPP_JSTATE_CANCELED;
	  job->completed = time(NULL);
	}

	cupsRWUnlock(&(client->printer->rwlock));

        serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_COMPLETED, NULL);

	serverRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_cancel_jobs()' - Cancel multiple jobs.
 */

static void
ipp_cancel_jobs(
    server_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*job_ids,	/* List of job-id's to cancel */
			*bad_job_ids = NULL;
					/* List of bad job-id values */
  const char		*username = NULL;/* Username */
  server_job_t		*job;		/* Current job pointer */
  cups_array_t		*to_cancel;	/* Jobs to cancel */
  ipp_op_t		op = ippGetOperation(client->request);
					/* Operation code */


 /*
  * See which user is canceling jobs...
  */

  if (Authentication)
  {
   /*
    * Use authenticated username...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (op == IPP_OP_CANCEL_MY_JOBS)
      username = client->username;
  }
  else if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) == NULL && op == IPP_OP_CANCEL_MY_JOBS)
  {
   /*
    * No authentication and no requesting-user-name...
    */

    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Need requesting-user-name with Cancel-My-Jobs.");
    return;
  }
  else if (op == IPP_OP_CANCEL_MY_JOBS)
  {
   /*
    * Use requesting-user-name value...
    */

    username = ippGetString(attr, 0, NULL);
  }

  if (op == IPP_OP_CANCEL_JOBS)
  {
    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }
  else
  {
    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Cancel-My-Jobs username='%s'", username);
  }

 /*
  * and then see if a list of jobs was provided...
  */

  job_ids = ippFindAttribute(client->request, "job-ids", IPP_TAG_INTEGER);

 /*
  * OK, cancel jobs on this printer...
  */

  cupsRWLockRead(&(client->printer->rwlock));

  to_cancel = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

  if (job_ids)
  {
   /*
    * Look for the specified jobs...
    */

    size_t		i,		/* Looping var */
			count;		/* Number of job-ids values */
    server_job_t	key;		/* Search key for jobs */

    for (i = 0, count = ippGetCount(job_ids); i < count; i ++)
    {
      key.id = ippGetInteger(job_ids, i);

      if ((job = (server_job_t *)cupsArrayFind(client->printer->jobs, &key)) != NULL)
      {
       /*
	* Validate this job...
	*/

	if (username && strcasecmp(username, job->username))
	{
	  if (!bad_job_ids)
	  {
	    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Job #%d is owned by another user.", job->id);

	    bad_job_ids = ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", job->id);
	  }
	  else
	    ippSetInteger(client->response, &bad_job_ids, ippGetCount(bad_job_ids), job->id);
	}
	else if (job->state >= IPP_JSTATE_CANCELED)
	{
	  if (!bad_job_ids)
	  {
	    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d cannot be canceled.", job->id);

	    bad_job_ids = ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", job->id);
	  }
	  else
	    ippSetInteger(client->response, &bad_job_ids, ippGetCount(bad_job_ids), job->id);
	}
	else
	  cupsArrayAdd(to_cancel, job);
      }
      else if (!bad_job_ids)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job #%d does not exist.", key.id);

	bad_job_ids = ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", key.id);
      }
      else
	ippSetInteger(client->response, &bad_job_ids, ippGetCount(bad_job_ids), key.id);
    }
  }
  else
  {
   /*
    * Look for jobs belonging to the requesting user...
    */

    for (job = (server_job_t *)cupsArrayGetFirst(client->printer->jobs); job; job = (server_job_t *)cupsArrayGetNext(client->printer->jobs))
    {
      if (job->state < IPP_JSTATE_CANCELED && (op == IPP_OP_CANCEL_JOBS || (username && !strcasecmp(username, job->username))))
        cupsArrayAdd(to_cancel, job);
    }
  }

  if (!bad_job_ids)
  {
   /*
    * If we got this far then we have a valid list of jobs to cancel...
    */

    for (job = (server_job_t *)cupsArrayGetFirst(to_cancel); job; job = (server_job_t *)cupsArrayGetNext(to_cancel))
    {
      if (job->state == IPP_JSTATE_PROCESSING || (job->state == IPP_JSTATE_HELD && job->fd >= 0))
      {
	job->cancel = 1;

	serverStopJob(job);
      }
      else
      {
	job->state     = IPP_JSTATE_CANCELED;
	job->completed = time(NULL);
      }

      serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_COMPLETED, NULL);
    }

    serverRespondIPP(client, IPP_STATUS_OK, NULL);
  }

  cupsArrayDelete(to_cancel);

  cupsRWUnlock(&(client->printer->rwlock));
}


/*
 * 'ipp_cancel_resource()' - Cancel a resource.
 */

static void
ipp_cancel_resource(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  ipp_attribute_t	*attr;		/* Request attribute */
  int			resource_id;	/* resource-id value */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "resource-id", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-id' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (resource_id = ippGetInteger(attr, 0)) < 1)
  {
    serverRespondUnsupported(client, attr);
    return;
  }
  else if ((resource = serverFindResourceById(resource_id)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Resource #%d not found.", resource_id);
    return;
  }
  else if (resource->state >= IPP_RSTATE_CANCELED)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Resource #%d already %s.", resource_id, resource->state == IPP_RSTATE_CANCELED ? "canceled" : "aborted");
    return;
  }

 /*
  * Set state to canceled...
  */

  if (resource->use > 0)
  {
    resource->cancel = 1;
  }
  else
  {
    const char *message = ippGetString(ippFindAttribute(client->request, "message", IPP_TAG_TEXT), 0, NULL);

    if (message)
      serverSetResourceState(resource, IPP_RSTATE_CANCELED, "%s", message);
    else
      serverSetResourceState(resource, IPP_RSTATE_CANCELED, NULL);
  }

 /*
  * Send attributes...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_cancel_subscription()' - Cancel a subscription.
 */

static void
ipp_cancel_subscription(
    server_client_t *client)		/* I - Client */
{
  server_subscription_t	*sub;		/* Subscription */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((sub = serverFindSubscription(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, sub->username, SERVER_GROUP_NONE, SubscriptionPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this subscription.");
    return;
  }

  cupsRWLockWrite(&SubscriptionsRWLock);
  cupsArrayRemove(Subscriptions, sub);
  serverDeleteSubscription(sub);
  cupsRWUnlock(&SubscriptionsRWLock);
  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_close_job()' - Close an open job.
 */

static void
ipp_close_job(server_client_t *client)	/* I - Client */
{
  server_job_t	*job;			/* Job information */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  switch (job->state)
  {
    case IPP_JSTATE_CANCELED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is canceled - can\'t close.", job->id);
        break;

    case IPP_JSTATE_ABORTED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is aborted - can\'t close.", job->id);
        break;

    case IPP_JSTATE_COMPLETED :
	serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
		    "Job #%d is completed - can\'t close.", job->id);
        break;

    default :
	serverRespondIPP(client, IPP_STATUS_OK, NULL);
        break;
  }
}


/*
 * 'ipp_create_job()' - Create a job object.
 */

static void
ipp_create_job(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* New job */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*hold_until;	/* job-hold-until-xxx attribute, if any */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if (!client->printer->is_accepting)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting jobs.");
    return;
  }

 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
    return;

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unexpected document data following request.");
    return;
  }

 /*
  * Create the job...
  */

  if ((job = serverCreateJob(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_TOO_MANY_JOBS, "Too many jobs are queued.");
    return;
  }

  if ((hold_until = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    hold_until = ippFindAttribute(client->request, "job-hold-until-time", IPP_TAG_DATE);

  if (hold_until || (job->printer->state_reasons & SERVER_PREASON_HOLD_NEW_JOBS))
    serverHoldJob(job, hold_until);

 /*
  * Return the job info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra, NULL);
  cupsArrayDelete(ra);

 /*
  * Add any subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_create_printer()' - Create a new printer.
 */

static void
ipp_create_printer(
    server_client_t *client)		/* I - Client connection */
{
  size_t		i,		/* Looping var */
			count;		/* Number of values */
  ipp_attribute_t	*attr,		/* Request attribute */
			*resource_ids,	/* resources-ids attribute */
			*supported;	/* Supported attribute */
  int			resource_id;	/* Resource ID value */
  server_resource_t	*resource;	/* Resource */
  const char		*service_type,	/* printer-service-type value */
			*printer_name,	/* printer-name value */
			*group;		/* auth-xxx-group value */
  char			name[128],	/* Sanitized printer name */
			*nameptr,	/* Pointer into name */
			path[256];	/* Resource path */
  server_pinfo_t	pinfo;		/* Printer information */
  cups_array_t		*ra;		/* Response attributes */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((resource_ids = ippFindAttribute(client->request, "resource-ids", IPP_TAG_INTEGER)) != NULL)
  {
    if (ippGetGroupTag(resource_ids) != IPP_TAG_OPERATION)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The 'resource-ids' attribute is not in the operation group.");
      serverRespondUnsupported(client, resource_ids);
      return;
    }
    else if ((count = ippGetCount(resource_ids)) > SERVER_RESOURCES_MAX)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Too many resources (%u) specified.", (unsigned)count);
      serverRespondUnsupported(client, resource_ids);
      return;
    }

    for (i = 0; i < count; i ++)
    {
      resource_id = ippGetInteger(resource_ids, i);

      if ((resource = serverFindResourceById(resource_id)) == NULL)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d not found.", resource_id);
	serverRespondUnsupported(client, resource_ids);
	return;
      }
      else if (resource->state != IPP_RSTATE_INSTALLED)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is not installed (%s).", resource_id, ippEnumString("resource-state", (int)resource->state));
	serverRespondUnsupported(client, resource_ids);
	return;
      }
      else if (!strcmp(resource->type, "template-printer"))
      {
	cupsRWLockRead(&SystemRWLock);
	supported = ippFindAttribute(SystemAttributes, "printer-creation-attributes-supported", IPP_TAG_KEYWORD);
	cupsRWUnlock(&SystemRWLock);

        if (!apply_template_attributes(client->request, IPP_TAG_PRINTER, resource, supported, sizeof(printer_values) / sizeof(printer_values[0]), printer_values))
        {
          serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to apply template-printer resource #%d: %s", resource_id, cupsLastErrorString());
	  return;
        }
      }
      else if (!strncmp(resource->type, "template-", 9))
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is the wrong type (%s).", resource_id, resource->type);
	serverRespondUnsupported(client, resource_ids);
	return;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-service-type", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'printer-service-type' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || (service_type = ippGetString(attr, 0, NULL)) == NULL || (strcmp(service_type, "print") && strcmp(service_type, "print3d")))
  {
    serverRespondUnsupported(client, attr);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-name", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'printer-name' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || (ippGetValueTag(attr) != IPP_TAG_NAME && ippGetValueTag(attr) != IPP_TAG_NAMELANG) || ippGetCount(attr) != 1 || (printer_name = ippGetString(attr, 0, NULL)) == NULL)
  {
    serverRespondUnsupported(client, attr);
    return;
  }

  cupsCopyString(name, printer_name, sizeof(name));
  for (nameptr = name; *nameptr; nameptr ++)
    if ((*nameptr & 255) <= ' ' || *nameptr == '#' || *nameptr == '/' || *nameptr == 0x7f)
      *nameptr = '_';

  snprintf(path, sizeof(path), "/ipp/%s/%s", service_type, name);

  if (serverFindPrinter(path))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "A printer named '%s' already exists.", name);
    return;
  }

  if (!valid_values(client, IPP_TAG_PRINTER, ippFindAttribute(SystemAttributes, "printer-creation-attributes-supported", IPP_TAG_KEYWORD), sizeof(printer_values) / sizeof(printer_values[0]), printer_values))
    return;

#ifndef _WIN32
  if ((attr = ippFindAttribute(client->request, "smi2699-auth-print-group", IPP_TAG_NAME)) == NULL)
    attr = ippFindAttribute(client->request, "smi2699-auth-proxy-group", IPP_TAG_NAME);

  if (attr && (group = ippGetString(attr, 0, NULL)) != NULL && !getgrnam(group))
  {
    serverRespondUnsupported(client, attr);
    return;
  }
#endif /* !_WIN32 */

  if ((attr = ippFindAttribute(client->request, "smi2699-device-command", IPP_TAG_NAME)) != NULL)
  {
    cupsRWLockRead(&SystemRWLock);
    supported = ippFindAttribute(SystemAttributes, "smi2699-device-command-supported", IPP_TAG_NAME);
    cupsRWUnlock(&SystemRWLock);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      return;
    }
  }

  if ((attr = ippFindAttribute(client->request, "smi2699-device-format", IPP_TAG_MIMETYPE)) != NULL)
  {
    cupsRWLockRead(&SystemRWLock);
    supported = ippFindAttribute(SystemAttributes, "smi2699-device-format-supported", IPP_TAG_MIMETYPE);
    cupsRWUnlock(&SystemRWLock);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      return;
    }
  }

  if ((attr = ippFindAttribute(client->request, "smi2699-device-uri", IPP_TAG_URI)) != NULL)
  {
    http_uri_status_t uri_status;	/* Decoding status */
    char	dscheme[32],		/* URI scheme */
		duserpass[256],		/* URI username:password */
		dhost[256],		/* URI host name */
		dpath[256];		/* URI resource path */
    int		dport;			/* URI port */

    cupsRWLockRead(&SystemRWLock);
    supported = ippFindAttribute(SystemAttributes, "smi2699-device-uri-schemes-supported", IPP_TAG_URISCHEME);
    cupsRWUnlock(&SystemRWLock);

    if ((uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(attr, 0, NULL), dscheme, sizeof(dscheme), duserpass, sizeof(duserpass), dhost, sizeof(dhost), &dport, dpath, sizeof(dpath))) < HTTP_URI_STATUS_OK)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad smi2699-device-uri: %s", httpURIStatusString(uri_status));
      serverRespondUnsupported(client, attr);
    }
    else if (!ippContainsString(supported, dscheme))
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_URI_SCHEME, "Unsupported smi2699-device-uri scheme '%s'.", dscheme);
      serverRespondUnsupported(client, attr);
      return;
    }
  }

 /*
  * Create the printer...
  */

  memset(&pinfo, 0, sizeof(pinfo));
  pinfo.attrs       = ippNew();
  pinfo.print_group = SERVER_GROUP_NONE;
  pinfo.proxy_group = SERVER_GROUP_NONE;

  serverCopyAttributes(pinfo.attrs, client->request, NULL, NULL, IPP_TAG_PRINTER, 0);

  for (attr = ippFirstAttribute(pinfo.attrs); attr; attr = ippNextAttribute(pinfo.attrs))
  {
    const char		*aname = ippGetName(attr);
					/* Attribute name */
#ifndef _WIN32
    struct group	*grp;		/* Group info */
#endif /* !_WIN32 */

    if (!aname)
      continue;

#ifndef _WIN32
    if (!strcmp(aname, "smi2699-auth-print-group"))
    {
      if ((grp = getgrnam(ippGetString(attr, 0, NULL))) != NULL)
        pinfo.print_group = grp->gr_gid;
    }
    else if (!strcmp(aname, "smi2699-auth-proxy-group"))
    {
      if ((grp = getgrnam(ippGetString(attr, 0, NULL))) != NULL)
        pinfo.proxy_group = grp->gr_gid;
    }
    else
#endif /* !_WIN32 */
    if (!strcmp(aname, "smi2699-device-command"))
    {
      pinfo.command = (char *)ippGetString(attr, 0, NULL);
    }
    else if (!strcmp(aname, "smi2699-device-format"))
    {
      pinfo.output_format = (char *)ippGetString(attr, 0, NULL);
    }
    else if (!strcmp(aname, "smi2699-device-uri"))
    {
      pinfo.device_uri = (char *)ippGetString(attr, 0, NULL);
    }
    else if (!strcmp(aname, "smi2699-max-output-device"))
    {
      pinfo.max_devices = ippGetInteger(attr, 0);
    }
  }

  if ((client->printer = serverCreatePrinter(path, name, printer_name, &pinfo, 1)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create printer.");
    return;
  }

  if (resource_ids)
  {
   /*
    * Allocate non-template resources to the printer...
    */

    count = ippGetCount(resource_ids);
    for (i = 0; i < count; i ++)
    {
      resource_id = ippGetInteger(resource_ids, i);
      resource    = serverFindResourceById(resource_id);

      if (strcmp(resource->type, "template-printer"))
        serverAllocatePrinterResource(client->printer, resource);
    }
  }

  serverAddPrinter(client->printer);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWLockRead(&client->printer->rwlock);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "printer-id");
  cupsArrayAdd(ra, "printer-is-accepting-jobs");
  cupsArrayAdd(ra, "printer-state");
  cupsArrayAdd(ra, "printer-state-reasons");
  cupsArrayAdd(ra, "printer-uuid");
  cupsArrayAdd(ra, "printer-xri-supported");
  cupsArrayAdd(ra, "system-state");
  cupsArrayAdd(ra, "system-state-reasons");

  serverCopyAttributes(client->response, client->printer->pinfo.attrs, ra, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO);
  copy_printer_state(client->response, client->printer, ra);

  cupsRWUnlock(&client->printer->rwlock);

 /*
  * Add any subscriptions...
  */

  ipp_create_xxx_subscriptions(client);

 /*
  * Add system state at the end...
  */

  copy_system_state(client->response, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_create_resource()' - Create a resource on the system.
 */

static void
ipp_create_resource(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*attr;		/* Request attribute */
  const char		*type,		/* Resource type keyword */
			*info,		/* Resource info text */
			*name,		/* Resource name text */
			*language;	/* Resource language text */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "resource-type", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-type' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || (type = ippGetString(attr, 0, NULL)) == NULL)
  {
    serverRespondUnsupported(client, attr);
    return;
  }
  else
  {
    ipp_attribute_t	*supported;	/* Supported values */

    cupsRWLockRead(&SystemRWLock);
    supported = ippFindAttribute(SystemAttributes, "resource-type-supported", IPP_TAG_KEYWORD);
    cupsRWUnlock(&SystemRWLock);

    if (!ippContainsString(supported, type))
    {
      serverRespondUnsupported(client, attr);
      return;
    }
  }

  if ((attr = ippFindAttribute(client->request, "resource-info", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(attr) != IPP_TAG_RESOURCE || ippGetValueTag(attr) != IPP_TAG_TEXT || ippGetCount(attr) != 1))
  {
    serverRespondUnsupported(client, attr);
    return;
  }

  info = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "resource-name", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(attr) != IPP_TAG_RESOURCE || ippGetValueTag(attr) != IPP_TAG_NAME || ippGetCount(attr) != 1))
  {
    serverRespondUnsupported(client, attr);
    return;
  }

  name = ippGetString(attr, 0, NULL);

  if ((attr = ippFindAttribute(client->request, "resource-natural-language", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(attr) != IPP_TAG_RESOURCE || ippGetValueTag(attr) != IPP_TAG_LANGUAGE || ippGetCount(attr) != 1))
  {
    serverRespondUnsupported(client, attr);
    return;
  }

  language = ippGetString(attr, 0, NULL);

 /*
  * Create an empty resource...
  */

  resource = serverCreateResource(NULL, NULL, NULL, name, info, type, language);

 /*
  * Return the resource info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  if (!strcmp(type, "static-icc-profile"))
  {
    ippAddString(client->response, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "resource-format-accepted", NULL, "application/vnd.iccprofile");
  }
  else if (!strcmp(type, "static-image"))
  {
    static const char * const formats[] = { "image/jpeg", "image/png" };

    ippAddStrings(client->response, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "resource-format-accepted", 2, NULL, formats);
  }
  else if (!strcmp(type, "static-strings"))
  {
    ippAddString(client->response, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "resource-format-accepted", NULL, "text/strings");
  }
  else
  {
   /*
    * template-document/job/printer
    */

    ippAddString(client->response, IPP_TAG_OPERATION, IPP_CONST_TAG(IPP_TAG_MIMETYPE), "resource-format-accepted", NULL, "application/ipp");
  }

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "resource-id");
  cupsArrayAdd(ra, "resource-state");
  cupsArrayAdd(ra, "resource-state-reasons");
  cupsArrayAdd(ra, "resource-uuid");

  copy_resource_attributes(client, resource, ra);
  cupsArrayDelete(ra);

 /*
  * Add any subscriptions...
  */

  client->resource = resource;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_create_xxx_subscriptions()' - Create subscriptions.
 */

static void
ipp_create_xxx_subscriptions(
    server_client_t *client)		/* I - Client connection */
{
  server_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*attr;		/* Subscription attribute */
  const char		*username;	/* requesting-user-name or
					   authenticated username */
  int			num_subs = 0,	/* Number of subscriptions */
			ok_subs = 0;	/* Number of good subscriptions */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

 /*
  * Get the target for the subscription...
  */

  if (ippGetOperation(client->request) == IPP_OP_CREATE_JOB_SUBSCRIPTIONS && !client->job)
  {
    int	job_id;				/* Job ID */

    if ((attr = ippFindAttribute(client->request, "notify-job-id", IPP_TAG_ZERO)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'notify-job-id' attribute in Create-Job-Subscriptions request.");
      return;
    }
    else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (job_id = ippGetInteger(attr, 0)) < 1)
    {
      serverRespondUnsupported(client, attr);
      return;
    }
    else if ((client->job = serverFindJob(client, job_id)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job #%d not found.", job_id);
      return;
    }
  }
  else if (ippGetOperation(client->request) == IPP_OP_CREATE_RESOURCE_SUBSCRIPTIONS && !client->resource)
  {
    int	resource_id;			/* Resource ID */

    if ((attr = ippFindAttribute(client->request, "resource-id", IPP_TAG_ZERO)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'resource-id' attribute in Create-Resource-Subscriptions request.");
      return;
    }
    else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (resource_id = ippGetInteger(attr, 0)) < 1)
    {
      serverRespondUnsupported(client, attr);
      return;
    }
    else if ((client->resource = serverFindResourceById(resource_id)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Resource #%d not found.", resource_id);
      return;
    }
  }

 /*
  * For the Create-xxx-Subscriptions operations, queue up a successful-ok
  * response...
  */

  if (ippGetOperation(client->request) == IPP_OP_CREATE_JOB_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_RESOURCE_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS)
    serverRespondIPP(client, IPP_STATUS_OK, NULL);

 /*
  * Get the authenticated user name, if any...
  */

  if (client->username[0])
    username = client->username;
  else if ((attr = ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME)) != NULL && ippGetGroupTag(attr) == IPP_TAG_OPERATION && ippGetCount(attr) == 1)
    username = ippGetString(attr, 0, NULL);
  else
    username = "anonymous";

 /*
  * Skip past the initial attributes to the first subscription group.
  */

  attr = ippFirstAttribute(client->request);
  while (attr && ippGetGroupTag(attr) != IPP_TAG_SUBSCRIPTION)
    attr = ippNextAttribute(client->request);

  while (attr)
  {
    const char		*attrname,	/* Attribute name */
			*pullmethod = NULL;
    					/* notify-pull-method */
    ipp_attribute_t	*notify_attributes = NULL,
					/* notify-attributes */
			*notify_charset = NULL,
					/* notify_charset */
			*notify_events = NULL,
					/* notify-events */
			*notify_natural_language = NULL,
					/* notify-natural-language */
			*notify_user_data = NULL;
					/* notify-user-data */
    int			interval = 0,	/* notify-time-interval */
			lease = SERVER_NOTIFY_LEASE_DURATION_DEFAULT;
					/* notify-lease-duration */
    ipp_status_t	status = IPP_STATUS_OK;
					/* notify-status-code */

    num_subs ++;

    while (attr)
    {
      if ((attrname = ippGetName(attr)) == NULL)
        break;

      if (!strcmp(attrname, "notify-recipient-uri"))
      {
       /*
        * Push notifications not supported.
	*/

        status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	ippCopyAttribute(client->response, attr, 0);
      }
      else if (!strcmp(attrname, "notify-pull-method"))
      {
	pullmethod = ippGetString(attr, 0, NULL);

        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || !pullmethod || strcmp(pullmethod, "ippget"))
	{
          ippCopyAttribute(client->response, attr, 0);
	  pullmethod = NULL;
	  status     = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	}
      }
      else if (!strcmp(attrname, "notify-attributes"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}

	notify_attributes = attr;
      }
      else if (!strcmp(attrname, "notify-charset"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_CHARSET || ippGetCount(attr) != 1 ||
	    (strcmp(ippGetString(attr, 0, NULL), "us-ascii") && strcmp(ippGetString(attr, 0, NULL), "utf-8")))
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
	  notify_charset = attr;
      }
      else if (!strcmp(attrname, "notify-natural-language"))
      {
        if (ippGetValueTag(attr) !=  IPP_TAG_LANGUAGE || ippGetCount(attr) != 1 || strcmp(ippGetString(attr, 0, NULL), "en"))
        {
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
	  notify_natural_language = attr;
      }
      else if (!strcmp(attrname, "notify-user-data"))
      {
        size_t	datalen;		/* Length of data */

        if (ippGetValueTag(attr) != IPP_TAG_STRING || ippGetCount(attr) != 1 || !ippGetOctetString(attr, 0, &datalen) || datalen > 63)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
	  notify_user_data = attr;
      }
      else if (!strcmp(attrname, "notify-events"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          notify_events = attr;
      }
      else if (!strcmp(attrname, "notify-lease-duration"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          lease = ippGetInteger(attr, 0);
      }
      else if (!strcmp(attrname, "notify-time-interval"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          interval = ippGetInteger(attr, 0);
      }

      attr = ippNextAttribute(client->request);
    }

    if (status)
    {
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", status);
    }
    else if (!pullmethod)
    {
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", IPP_STATUS_ERROR_BAD_REQUEST);
    }
    else
    {
      if ((sub = serverCreateSubscription(client, interval, lease, username, notify_charset, notify_natural_language, notify_events, notify_attributes, notify_user_data)) != NULL)
      {
        ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-subscription-id", sub->id);
        ok_subs ++;
      }
      else
        ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", IPP_STATUS_ERROR_INTERNAL);
    }
  }

  if (ok_subs == 0 && num_subs != 0)
    ippSetStatusCode(client->response, IPP_STATUS_ERROR_IGNORED_ALL_SUBSCRIPTIONS);
  else if (ok_subs != num_subs)
    ippSetStatusCode(client->response, IPP_STATUS_OK_IGNORED_SUBSCRIPTIONS);
}


/*
 * 'ipp_deallocate_printer_resources()' - Deallocate resources for a printer.
 */

static void
ipp_deallocate_printer_resources(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer = client->printer;
					/* Printer */
  ipp_attribute_t	*resource_ids;	/* resource-ids attribute */
  size_t		i, j,		/* Looping vars */
			count;		/* Number of values */
  int			resource_id;	/* Current resource ID */
  server_resource_t	*resource;	/* Current resource */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((resource_ids = ippFindAttribute(client->request, "resource-ids", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing 'resource-ids' attribute.");
    return;
  }
  else if (ippGetGroupTag(resource_ids) != IPP_TAG_OPERATION)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The 'resource-ids' attribute is in the wrong group.");
    return;
  }
  else if (ippGetValueTag(resource_ids) != IPP_TAG_INTEGER)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "The 'resource-ids' attribute is the wrong type.");
    serverRespondUnsupported(client, resource_ids);
    return;
  }

  cupsRWLockWrite(&printer->rwlock);

  for (i = 0, count = ippGetCount(resource_ids); i < count; i ++)
  {
    resource_id = ippGetInteger(resource_ids, i);
    resource    = serverFindResourceById(resource_id);

    for (j = 0; j < printer->num_resources; j ++)
    {
      if (printer->resources[j] == resource_id)
	break;
    }

    if (!resource || j >= printer->num_resources)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is not allocated to the printer.", resource_id);
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&printer->rwlock);
      return;
    }
  }

 /*
  * Deallocate resources...
  */

  for (i = 0, count = ippGetCount(resource_ids); i < count; i ++)
  {
    resource_id = ippGetInteger(resource_ids, i);
    resource    = serverFindResourceById(resource_id);

    serverDeallocatePrinterResource(printer, resource);
  }

  cupsRWUnlock(&printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_delete_printer()' - Delete a printer.
 */

static void
ipp_delete_printer(
    server_client_t *client)		/* I - Client */
{
  server_job_t		*job;		/* Current job */
  server_subscription_t	*sub;		/* Current subscription */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockWrite(&PrintersRWLock);

  serverLogPrinter(SERVER_LOGLEVEL_DEBUG, client->printer, "Removing printer %d from printers list.", client->printer->id);

  cupsArrayRemove(Printers, client->printer);

  client->printer->is_deleted = 1;

 /*
  * Abort all jobs for this printer...
  */

  cupsRWLockWrite(&client->printer->rwlock);

  for (job = (server_job_t *)cupsArrayGetFirst(client->printer->active_jobs); job; job = (server_job_t *)cupsArrayGetNext(client->printer->active_jobs))
  {
    if (job->state == IPP_JSTATE_PENDING || job->state == IPP_JSTATE_HELD)
    {
      job->state = IPP_JSTATE_ABORTED;
      serverAddEventNoLock(job->printer, job, NULL, SERVER_EVENT_JOB_COMPLETED, "Job aborted because printer has been deleted.");
    }
  }

  cupsRWUnlock(&client->printer->rwlock);

 /*
  * Mark all subscriptions for this printer to expire in 30 seconds...
  */

  cupsRWLockRead(&SubscriptionsRWLock);

  for (sub = (server_subscription_t *)cupsArrayGetFirst(Subscriptions); sub; sub = (server_subscription_t *)cupsArrayGetNext(Subscriptions))
  {
    if (sub->printer == client->printer || (sub->job && sub->job->printer == client->printer))
    {
      sub->printer = NULL;
      sub->job     = NULL;
      sub->expire  = time(NULL) + 30;
    }
  }

  cupsRWUnlock(&SubscriptionsRWLock);

  if (client->printer->processing_job)
  {
   /*
    * Printer is processing a job, delete immediately...
    */

    client->printer->state_reasons |= SERVER_PREASON_MOVING_TO_PAUSED | SERVER_PREASON_DELETING;
    serverStopJob(client->printer->processing_job);

    serverAddEventNoLock(client->printer, NULL, NULL, SERVER_EVENT_PRINTER_STATE_CHANGED, "Printer being deleted.");
  }
  else
  {
    client->printer->state         = IPP_PSTATE_STOPPED;
    client->printer->state_reasons |= SERVER_PREASON_DELETING;

    serverAddEventNoLock(client->printer, NULL, NULL, SERVER_EVENT_PRINTER_DELETED, "Printer deleted.");

    serverDeletePrinter(client->printer);
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWUnlock(&PrintersRWLock);
}


/*
 * 'ipp_deregister_output_device()' - Unregister an output device.
 */

static void
ipp_deregister_output_device(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Find the device...
  */

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Output device not found.");
    return;
  }

 /*
  * Remove the device from the printer...
  */

  cupsRWLockWrite(&client->printer->rwlock);

  cupsArrayRemove(client->printer->pinfo.devices, device);

  serverUpdateDeviceAttributesNoLock(client->printer);
  serverUpdateDeviceStateNoLock(client->printer);

  cupsRWUnlock(&client->printer->rwlock);

 /*
  * Delete the device...
  */

  serverDeleteDevice(device);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_disable_all_printers()' - Stop accepting new jobs for all printers.
 */

static void
ipp_disable_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&SystemRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    serverDisablePrinter(printer);

  cupsRWUnlock(&SystemRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_disable_printer()' - Stop accepting new jobs for a printer.
 */

static void
ipp_disable_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  serverDisablePrinter(client->printer);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_enable_all_printers()' - Start accepting new jobs for all printers.
 */

static void
ipp_enable_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&SystemRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    serverEnablePrinter(printer);

  cupsRWUnlock(&SystemRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_enable_printer()' - Start accepting new jobs for a printer.
 */

static void
ipp_enable_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  serverEnablePrinter(client->printer);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_fetch_document()' - Download a document.
 */

static void
ipp_fetch_document(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */
  int			compression;	/* compression */
  char			filename[1024];	/* Job filename */
  const char		*format = NULL;	/* document-format */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "compression-accepted", IPP_TAG_KEYWORD)) != NULL)
    compression = !strcmp(ippGetString(attr, 0, NULL), "gzip");
  else
    compression = 0;

  if ((attr = ippFindAttribute(client->request, "document-format-accepted", IPP_TAG_MIMETYPE)) == NULL)
    attr = ippFindAttribute(client->printer->dev_attrs, "document-format-supported", IPP_TAG_MIMETYPE);

  if (attr && !ippContainsString(attr, job->format))
  {
    if (ippContainsString(attr, "image/urf"))
      format = "image/urf";
    else if (ippContainsString(attr, "image/pwg-raster"))
      format = "image/pwg-raster";
    else if (ippContainsString(attr, "application/vnd.hp-pcl"))
      format = "application/vnd.hp-pcl";
    else
      format = NULL;

    if (format)
    {
     /*
      * Transform and stream document as raster...
      */

      serverRespondIPP(client, IPP_STATUS_OK, NULL);
      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
      ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "compression", NULL, compression ? "gzip" : "none");

      if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
	httpFlush(client->http);	/* Flush trailing (junk) data */

      serverLogAttributes(client, "Response:", client->response, 2);

      serverLogClient(SERVER_LOGLEVEL_INFO, client, "%s", httpStatusString(HTTP_STATUS_OK));

      httpClearFields(client->http);
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");

      httpSetLength(client->http, 0);
      if (httpWriteResponse(client->http, HTTP_STATUS_OK) < 0)
	return;

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "ipp_fetch_document: Sending %d bytes of IPP response.", (int)ippLength(client->response));

      ippSetState(client->response, IPP_STATE_IDLE);

      if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      {
	serverLogClient(SERVER_LOGLEVEL_ERROR, client, "Unable to write IPP response.");
	return;
      }

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "ipp_fetch_document: Sent IPP response.");

      if (compression)
	httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, "gzip");

      job->state = IPP_JSTATE_PROCESSING;
      serverTransformJob(client, job, "ipptransform", format, SERVER_TRANSFORM_TO_CLIENT);

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "ipp_fetch_document: Sending 0-length chunk.");
      httpWrite(client->http, "", 0);

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "ipp_fetch_document: Flushing write buffer.");
      httpFlushWrite(client->http);
      return;
    }
    else
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FETCHABLE, "Document not available in requested format.");
      return;
    }
  }

  if (job->format)
  {
    serverCreateJobFilename(job, job->format, filename, sizeof(filename));

    if (access(filename, R_OK))
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FETCHABLE, "Document not available in requested format.");
      return;
    }

    format = job->format;
  }
  else
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FETCHABLE, "Document format unknown.");
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
  ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  ippAddString(client->response, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "compression", NULL, compression ? "gzip" : "none");

  client->fetch_file = open(filename, O_RDONLY | O_BINARY);
}


/*
 * 'ipp_fetch_job()' - Download a job.
 */

static void
ipp_fetch_job(server_client_t *client)	/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (job->dev_uuid && strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if (!(job->state_reasons & SERVER_JREASON_JOB_FETCHABLE))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FETCHABLE, "Job not fetchable.");
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
  copy_job_attributes(client, job, NULL, NULL);
}


/*
 * 'ipp_get_document_attributes()' - Get the attributes for a document object.
 *
 * Note: This implementation only supports single document jobs so we
 *       synthesize the information for a single document from the job.
 */

static void
ipp_get_document_attributes(
    server_client_t *client)		/* I - Client */
{
  server_job_t	*job;			/* Job */
  ipp_attribute_t *number;		/* document-number attribute */
  cups_array_t	*ra;			/* requested-attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if ((number = ippFindAttribute(client->request, "document-number", IPP_TAG_INTEGER)) == NULL || ippGetInteger(number, 0) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Document #%d not found.", ippGetInteger(number, 0));
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_doc_attributes(client, job, ra, serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, DocumentPrivacyScope) ? NULL : DocumentPrivacyArray);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_documents()' - Get the list of documents in a job.
 *
 * Note: This implementation only supports single document jobs so we
 *       synthesize the information for a single document from the job.
 */

static void
ipp_get_documents(server_client_t *client)/* I - Client */
{
  server_job_t	*job;			/* Job */
  cups_array_t	*ra;			/* requested-attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  copy_doc_attributes(client, job, ra, serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, DocumentPrivacyScope) ? NULL : DocumentPrivacyArray);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_job_attributes()' - Get the attributes for a job object.
 */

static void
ipp_get_job_attributes(
    server_client_t *client)		/* I - Client */
{
  server_job_t	*job;			/* Job */
  cups_array_t	*ra,			/* requested-attributes */
		*pa = NULL;		/* job-privacy-attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job not found.");
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);
  if (serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
    serverLogClient(SERVER_LOGLEVEL_INFO, client, "%s Job #%d attributes accessed by \"%s\".", job->printer->name, job->id, client->username);
  else
    pa = JobPrivacyArray;

  copy_job_attributes(client, job, ra, pa);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_jobs()' - Get a list of job objects.
 */

static void
ipp_get_jobs(server_client_t *client)	/* I - Client */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*which_jobs = NULL;
					/* which-jobs values */
  int			job_comparison;	/* Job comparison */
  ipp_jstate_t		job_state;	/* job-state value */
  server_jreason_t	job_reasons;	/* job-state-reasons values */
  int			first_job_id,	/* First job ID */
			limit,		/* Maximum number of jobs to return */
			count;		/* Number of jobs that match */
  const char		*username;	/* Username */
  server_job_t		*job;		/* Current job pointer */
  cups_array_t		*ra,		/* Requested attributes array */
			*pa;		/* Privacy attributes array */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

 /*
  * See if the "which-jobs" attribute have been specified...
  */

  if ((attr = ippFindAttribute(client->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL)
  {
    which_jobs = ippGetString(attr, 0, NULL);
    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Jobs which-jobs='%s'", which_jobs);
  }

  job_reasons = SERVER_JREASON_NONE;

  if (!which_jobs || !strcmp(which_jobs, "not-completed"))
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else if (!strcmp(which_jobs, "completed"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "aborted"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_ABORTED;
  }
  else if (!strcmp(which_jobs, "all"))
  {
    job_comparison = 1;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "canceled"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_CANCELED;
  }
  else if (!strcmp(which_jobs, "pending"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PENDING;
  }
  else if (!strcmp(which_jobs, "pending-held"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_HELD;
  }
  else if (!strcmp(which_jobs, "processing"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_PROCESSING;
  }
  else if (!strcmp(which_jobs, "processing-stopped"))
  {
    job_comparison = 0;
    job_state      = IPP_JSTATE_STOPPED;
  }
  else if (!strcmp(which_jobs, "fetchable") && client->printer->pinfo.proxy_group != SERVER_GROUP_NONE)
  {
    job_comparison = -1;
    job_state      = IPP_JSTATE_STOPPED;
    job_reasons    = SERVER_JREASON_JOB_FETCHABLE;
  }
  else
  {
    serverRespondUnsupported(client, attr);
    return;
  }

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(client->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Jobs limit=%d", limit);
  }
  else
    limit = 0;

  if ((attr = ippFindAttribute(client->request, "first-job-id",
                               IPP_TAG_INTEGER)) != NULL)
  {
    first_job_id = ippGetInteger(attr, 0);

    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Jobs first-job-id=%d", first_job_id);
  }
  else
    first_job_id = 1;

 /*
  * See if we only want to see jobs for a specific user...
  */

  username = NULL;

  if ((attr = ippFindAttribute(client->request, "my-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    int my_jobs = ippGetBoolean(attr, 0);

    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Jobs my-jobs=%s", my_jobs ? "true" : "false");

    if (my_jobs)
    {
      if ((attr = ippFindAttribute(client->request, "requesting-user-name",
					IPP_TAG_NAME)) == NULL)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST,
	            "Need requesting-user-name with my-jobs.");
	return;
      }

      username = ippGetString(attr, 0, NULL);

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Jobs requesting-user-name='%s'", username);
    }
  }

 /*
  * OK, build a list of jobs for this printer...
  */

  ra = ippCreateRequestedArray(client->request);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWLockRead(&(client->printer->rwlock));

  for (count = 0, job = (server_job_t *)cupsArrayGetFirst(client->printer->jobs);
       (limit <= 0 || count < limit) && job;
       job = (server_job_t *)cupsArrayGetNext(client->printer->jobs))
  {
   /*
    * Filter out jobs that don't match...
    */

    if (job->id < first_job_id || (username && job->username && strcasecmp(username, job->username)))
      continue;

    if (job_reasons != SERVER_JREASON_NONE)
    {
      if (!(job->state_reasons & job_reasons))
        continue;
    }
    else if ((job_comparison < 0 && job->state > job_state) ||
             (job_comparison == 0 && job->state != job_state) ||
             (job_comparison > 0 && job->state < job_state))
    {
      continue;
    }

    if (count > 0)
      ippAddSeparator(client->response);

    count ++;

    if (serverAuthorizeUser(client, job->username, job->printer->pinfo.proxy_group, JobPrivacyScope))
    {
      pa = NULL;
      serverLogClient(SERVER_LOGLEVEL_INFO, client, "%s Job #%d attributes accessed by \"%s\".", job->printer->name, job->id, client->username);
    }
    else
    {
      pa = JobPrivacyArray;
    }

    copy_job_attributes(client, job, ra, pa);
  }

  cupsArrayDelete(ra);

  cupsRWUnlock(&(client->printer->rwlock));
}


/*
 * 'ipp_get_notifications()' - Get notification events for one or more subscriptions.
 */

static void
ipp_get_notifications(
    server_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*sub_ids,	/* notify-subscription-ids */
			*seq_nums;	/* notify-sequence-numbers */
  bool			notify_wait;	/* Wait for events? */
  size_t		i,		/* Looping vars */
			count;		/* Number of IDs */
  int			seq_num;	/* Sequence number */
  server_subscription_t	*sub;		/* Current subscription */
  ipp_t			*event;		/* Current event */
  int			num_events = 0;	/* Number of events returned */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((sub_ids = ippFindAttribute(client->request, "notify-subscription-ids", IPP_TAG_INTEGER)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing notify-subscription-ids attribute.");
    return;
  }

  count       = ippGetCount(sub_ids);
  seq_nums    = ippFindAttribute(client->request, "notify-sequence-numbers", IPP_TAG_INTEGER);
  notify_wait = ippGetBoolean(ippFindAttribute(client->request, "notify-wait", IPP_TAG_BOOLEAN), 0);

  if (seq_nums && count != ippGetCount(seq_nums))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The notify-subscription-ids and notify-sequence-numbers attributes are different lengths.");
    return;
  }

  do
  {
    for (i = 0; i < count; i ++)
    {
      if ((sub = serverFindSubscription(client, ippGetInteger(sub_ids, i))) == NULL)
      {
        serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription #%d was not found.", ippGetInteger(sub_ids, i));
        ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "notify-subscription-ids", ippGetInteger(sub_ids, i));
	break;
      }

      if (!serverAuthorizeUser(client, sub->username, SERVER_GROUP_NONE, SubscriptionPrivacyScope))
      {
        serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "You do not have access to subscription #%d.", ippGetInteger(sub_ids, i));
        ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "notify-subscription-ids", ippGetInteger(sub_ids, i));
	break;
      }

      cupsRWLockRead(&sub->rwlock);

      seq_num = ippGetInteger(seq_nums, i);
      if (seq_num < sub->first_sequence)
	seq_num = sub->first_sequence;

      if (seq_num > sub->last_sequence)
      {
        cupsRWUnlock(&sub->rwlock);
	continue;
      }

      for (event = (ipp_t *)cupsArrayGetElement(sub->events, (size_t)(seq_num - sub->first_sequence)); event; event = (ipp_t *)cupsArrayGetNext(sub->events))
      {
	if (num_events == 0)
	{
	  serverRespondIPP(client, IPP_STATUS_OK, NULL);
	  ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-get-interval", 30);
	  if (client->printer)
	    ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - client->printer->start_time));
	  else
	    ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - SystemStartTime));
	}
	else
	  ippAddSeparator(client->response);

	ippCopyAttributes(client->response, event, 0, NULL, NULL);
	num_events ++;
      }

      cupsRWUnlock(&sub->rwlock);
    }

    if (i < count)
      break;
    else if (num_events == 0 && notify_wait)
    {
      if (notify_wait > 0)
      {
       /*
	* Wait for more events...
	*/

        serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Waiting for events.");

	cupsMutexLock(&NotificationMutex);
	cupsCondWait(&NotificationCondition, &NotificationMutex, 30.0);
	cupsMutexUnlock(&NotificationMutex);

        serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Done waiting for events.");

        notify_wait = -1;
      }
      else
      {
       /*
        * Stop waiting for events...
        */

        notify_wait = 0;
      }
    }
  }
  while (num_events == 0 && notify_wait);
}


/*
 * 'ipp_get_output_device_attributes()' - Get attributes for an output device.
 */

static void
ipp_get_output_device_attributes(
    server_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  server_device_t	*device;	/* Device */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Output device not found.");
    return;
  }

  ra = ippCreateRequestedArray(client->request);

  cupsRWLockRead(&device->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
  serverCopyAttributes(client->response, device->attrs, ra, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO);

  cupsRWUnlock(&device->rwlock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_printer_attributes()' - Get the attributes for a printer object.
 */

static void
ipp_get_printer_attributes(
    server_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  server_printer_t	*printer;	/* Printer */


 /*
  * Send the attributes...
  */

  ra      = ippCreateRequestedArray(client->request);
  printer = client->printer;

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWLockRead(&(printer->rwlock));

  copy_printer_attributes(client, printer, ra);

  cupsRWUnlock(&(printer->rwlock));

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_printer_supported_values()' - Return the supported values for
 *                                        the infrastructure printer.
 */

static void
ipp_get_printer_supported_values(
    server_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes */
  ipp_attribute_t	*settable,	/* Settable attributes */
			*supported;	/* Supported attributes */
  size_t		i,		/* Looping var */
			count;		/* Number of settable attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  settable = ippFindAttribute(client->printer->pinfo.attrs, "printer-settable-attributes-supported", IPP_TAG_KEYWORD);
  count    = ippGetCount(settable);
  ra       = ippCreateRequestedArray(client->request);

  for (i = 0; i < count; i ++)
  {
    const char *name = ippGetString(settable, i, NULL);
					/* Settable attribute name */

    if (!ra || cupsArrayFind(ra, (void *)name))
    {
      if ((supported = ippFindAttribute(client->printer->pinfo.attrs, name, IPP_TAG_ZERO)) != NULL)
        ippCopyAttribute(client->response, supported, 0);
      else
        ippAddOutOfBand(client->response, IPP_TAG_PRINTER, IPP_TAG_ADMINDEFINE, name);
    }
  }

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_printers()' - Get a list of printers.
 */

static void
ipp_get_printers(
    server_client_t *client)		/* I - Client */
{
  int			i,		/* Looping var */
			count;		/* Number of printers returned */
  server_printer_t	*printer;	/* Current printer */
  ipp_attribute_t	*printer_ids;	/* printer-ids operation attribute, if any */
  int			first_index,	/* first-index operation attribute value, if any */
			limit;		/* limit operation attribute value, if any */
  const char		*geo_location,	/* printer-geo-location value, if any */
			*location,	/* printer-location value, if any */
			*service_type,	/* printer-service-type value, if any */
			*document_format,/* document-format value, if any */
			*which_printers;/* which-printers value, if any */
  float			geo_distance = 30.0;
					/* Distance for geographic filter */
  cups_array_t		*ra;		/* requested-attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  printer_ids     = ippFindAttribute(client->request, "printer-ids", IPP_TAG_INTEGER);
  first_index     = ippGetInteger(ippFindAttribute(client->request, "first-index", IPP_TAG_INTEGER), 0);
  limit           = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  geo_location    = ippGetString(ippFindAttribute(client->request, "printer-geo-location", IPP_TAG_URI), 0, NULL);
  location        = ippGetString(ippFindAttribute(client->request, "printer-location", IPP_TAG_TEXT), 0, NULL);
  service_type    = ippGetString(ippFindAttribute(client->request, "printer-service-type", IPP_TAG_KEYWORD), 0, NULL);
  document_format = ippGetString(ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE), 0, NULL);
  which_printers  = ippGetString(ippFindAttribute(client->request, "which-printers", IPP_TAG_KEYWORD), 0, NULL);

  if (first_index <= 0)
    first_index = 1;

  if (geo_location)
  {
   /*
    * Determine how close the printer needs to be...
    */

    const char	*u = strstr(geo_location, "u=");
					/* Uncertainty value from URI */

    if (u)
      geo_distance = (float)atof(u + 2);
  }

  if (which_printers)
  {
    if (!strcmp(which_printers, "all"))
    {
      which_printers = NULL;
    }
    else if (!strcmp(which_printers, "shutdown") || !strcmp(which_printers, "testing"))
    {
      serverRespondIPP(client, IPP_STATUS_OK, NULL);
      return;
    }
  }

  ra = ippCreateRequestedArray(client->request);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWLockRead(&PrintersRWLock);

  for (i = 0, count = 0, printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
  {
    const char	*printer_geo_location;	/* Printer's geo-location value */

    cupsRWLockRead(&printer->rwlock);

    if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    if (printer_ids && !ippContainsInteger(printer_ids, printer->id))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    printer_geo_location = ippGetString(ippFindAttribute(printer->pinfo.attrs, "printer-geo-location", IPP_TAG_URI), 0, NULL);

    if (geo_location && (!printer_geo_location || wgs84_distance(printer_geo_location, geo_location) > geo_distance))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    if (location && (!printer->pinfo.location || strcmp(printer->pinfo.location, location)))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    if (document_format && !ippContainsString(ippFindAttribute(printer->pinfo.attrs, "document-format-supported", IPP_TAG_MIMETYPE), document_format))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    if (service_type && ((!strcmp(service_type, "print") && printer->type != SERVER_TYPE_PRINT) || (!strcmp(service_type, "print3d") && printer->type != SERVER_TYPE_PRINT3D) || (strcmp(service_type, "print") && strcmp(service_type, "print3d"))))
    {
      cupsRWUnlock(&printer->rwlock);
      continue;
    }

    if (which_printers)
    {
     /*
      * Values are 'accepting', 'all', 'idle', 'not-accepting', 'processing',
      * 'shutdown', 'stopped', and 'testing'.  The 'all' value gets filtered
      * out, and right now 'shutdown' and 'testing' are not supported.
      */

      if ((!strcmp(which_printers, "accepting") && !printer->is_accepting) ||
          (!strcmp(which_printers, "idle") && printer->state != IPP_PSTATE_IDLE) ||
          (!strcmp(which_printers, "not-accepting") && printer->is_accepting) ||
          (!strcmp(which_printers, "processing") && printer->state != IPP_PSTATE_PROCESSING) ||
          (!strcmp(which_printers, "stopped") && printer->state != IPP_PSTATE_STOPPED))
      {
	cupsRWUnlock(&printer->rwlock);
	continue;
      }
    }

   /*
    * Whew, if we got this far we probably want to send this printer's info.
    * Check whether the client specifies first-index/limit...
    */

    i ++;
    if (i < first_index)
      continue;

    if (count)
      ippAddSeparator(client->response);

    copy_printer_attributes(client, printer, ra);

    count ++;

    cupsRWUnlock(&printer->rwlock);

    if (limit > 0 && count >= limit)
      break;
  }

  cupsRWUnlock(&PrintersRWLock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_resource_attributes()' - Get resource attributes.
 */

static void
ipp_get_resource_attributes(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*attr;		/* Request attribute */
  int			resource_id;	/* resource-id value */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "resource-id", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-id' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (resource_id = ippGetInteger(attr, 0)) < 1)
  {
    serverRespondUnsupported(client, attr);
    return;
  }
  else if ((resource = serverFindResourceById(resource_id)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Resource #%d not found.", resource_id);
    return;
  }

 /*
  * Send attributes...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);

  cupsRWLockRead(&resource->rwlock);
  copy_resource_attributes(client, resource, ra);
  cupsRWUnlock(&resource->rwlock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_resources()' - Get resources.
 */

static void
ipp_get_resources(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*attr,		/* Request attribute */
			*resource_formats,
					/* resource-formats attribute */
			*resource_ids,	/* resource-ids attribute */
			*resource_states,
					/* resource-states attribute */
			*resource_types;/* resource-types attribute */
  int			first_index,	/* First resource index */
			limit,		/* Maximum number of jobs to return */
			idx,		/* Index */
			count;		/* Number of jobs that match */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "first-index", IPP_TAG_INTEGER)) != NULL)
  {
    first_index = ippGetInteger(attr, 0);

    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Resources first-index=%d", first_index);
  }
  else
    first_index = 1;

  if ((attr = ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER)) != NULL)
  {
    limit = ippGetInteger(attr, 0);

    serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "Get-Resources limit=%d", limit);
  }
  else
    limit = 0;

  if ((resource_formats = ippFindAttribute(client->request, "resource-formats", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(resource_formats) != IPP_TAG_OPERATION || ippGetValueTag(resource_formats) != IPP_TAG_MIMETYPE)
    {
      serverRespondUnsupported(client, resource_formats);
      httpFlush(client->http);
      return;
    }
  }

  if ((resource_ids = ippFindAttribute(client->request, "resource-ids", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(resource_ids) != IPP_TAG_OPERATION || ippGetValueTag(resource_ids) != IPP_TAG_INTEGER)
    {
      serverRespondUnsupported(client, resource_ids);
      httpFlush(client->http);
      return;
    }
  }

  if ((resource_states = ippFindAttribute(client->request, "resource-states", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(resource_states) != IPP_TAG_OPERATION || ippGetValueTag(resource_states) != IPP_TAG_ENUM)
    {
      serverRespondUnsupported(client, resource_states);
      httpFlush(client->http);
      return;
    }
  }

  if ((resource_types = ippFindAttribute(client->request, "resource-types", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(resource_types) != IPP_TAG_OPERATION || ippGetValueTag(resource_types) != IPP_TAG_KEYWORD)
    {
      serverRespondUnsupported(client, resource_types);
      httpFlush(client->http);
      return;
    }
  }

 /*
  * Send attributes...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);

  cupsRWLockRead(&ResourcesRWLock);

  for (resource = (server_resource_t *)cupsArrayGetFirst(ResourcesById), count = 0, idx = 0; (limit <= 0 || count < limit) && resource; resource = (server_resource_t *)cupsArrayGetNext(ResourcesById))
  {
    cupsRWLockRead(&resource->rwlock);

    if ((!resource_formats || ippContainsString(resource_formats, resource->format)) &&
        (!resource_ids || ippContainsInteger(resource_ids, resource->id)) &&
        (!resource_states || ippContainsInteger(resource_states, (int)resource->state)) &&
        (!resource_types || ippContainsString(resource_types, resource->type)))
    {
      idx ++;
      if (idx >= first_index)
      {
        copy_resource_attributes(client, resource, ra);
        count ++;
      }
    }

    cupsRWUnlock(&resource->rwlock);
  }

  cupsRWUnlock(&ResourcesRWLock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_subscription_attributes()' - Get attributes for a subscription.
 */

static void
ipp_get_subscription_attributes(
    server_client_t *client)		/* I - Client */
{
  server_subscription_t	*sub;		/* Subscription */
  cups_array_t		*ra = ippCreateRequestedArray(client->request),
					/* Requested attributes */
			*pa = NULL;	/* Privacy attributes */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }
  else if (Authentication && !client->printer && !serverAuthorizeUser(client, NULL, SERVER_GROUP_NONE, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this system.");
    return;
  }

  if ((sub = serverFindSubscription(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
  }
  else
  {
    serverRespondIPP(client, IPP_STATUS_OK, NULL);

    if (serverAuthorizeUser(client, sub->username, SERVER_GROUP_NONE, SubscriptionPrivacyScope))
      serverLogClient(SERVER_LOGLEVEL_INFO, client, "Subscription #%d attributes accessed by \"%s\".", sub->id, client->username);
    else
      pa = SubscriptionPrivacyArray;

    copy_subscription_attributes(client, sub, ra, pa);
  }

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_subscriptions()' - Get attributes for all subscriptions.
 */

static void
ipp_get_subscriptions(
    server_client_t *client)		/* I - Client */
{
  server_subscription_t	*sub;		/* Current subscription */
  cups_array_t		*ra = ippCreateRequestedArray(client->request),
					/* Requested attributes */
			*pa;		/* Privacy attributes */
  int			job_id,		/* notify-job-id value */
			limit,		/* limit value, if any */
			my_subs,	/* my-subscriptions value */
			count = 0;	/* Number of subscriptions reported */
  const char		*username;	/* Most authenticated user name */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }
  else if (Authentication && !client->printer && !serverAuthorizeUser(client, NULL, SERVER_GROUP_NONE, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this system.");
    return;
  }

  job_id  = ippGetInteger(ippFindAttribute(client->request, "notify-job-id", IPP_TAG_INTEGER), 0);
  limit   = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  my_subs = ippGetBoolean(ippFindAttribute(client->request, "my-subscriptions", IPP_TAG_BOOLEAN), 0);

  if (client->username[0])
    username = client->username;
  else if ((username = ippGetString(ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    username = "anonymous";

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
  cupsRWLockRead(&SubscriptionsRWLock);
  for (sub = (server_subscription_t *)cupsArrayGetFirst(Subscriptions); sub; sub = (server_subscription_t *)cupsArrayGetNext(Subscriptions))
  {
    if ((job_id > 0 && (!sub->job || sub->job->id != job_id)) || (job_id <= 0 && sub->job))
      continue;

    if (my_subs && strcmp(username, sub->username))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    if (serverAuthorizeUser(client, sub->username, SERVER_GROUP_NONE, SubscriptionPrivacyScope))
    {
      pa = NULL;
      serverLogClient(SERVER_LOGLEVEL_INFO, client, "Subscription #%d attributes accessed by \"%s\".", sub->id, client->username);
    }
    else
      pa = SubscriptionPrivacyArray;

    copy_subscription_attributes(client, sub, ra, pa);

    count ++;
    if (limit > 0 && count >= limit)
      break;
  }
  cupsRWUnlock(&SubscriptionsRWLock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_get_system_attributes()' - Get the attributes for the system object.
 */

static void
ipp_get_system_attributes(
    server_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Send the attributes...
  */

  ra = ippCreateRequestedArray(client->request);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  cupsRWLockRead(&SystemRWLock);

  serverCopyAttributes(client->response, SystemAttributes, ra, NULL, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);
//  serverCopyAttributes(client->response, PrivacyAttributes, ra, NULL, IPP_TAG_ZERO, IPP_TAG_CUPS_CONST);

  if (!ra || cupsArrayFind(ra, "system-config-change-date-time"))
    ippAddDate(client->response, IPP_TAG_SYSTEM, "system-config-change-date-time", ippTimeToDate(SystemConfigChangeTime));

  if (!ra || cupsArrayFind(ra, "system-config-change-time"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-config-change-time", (int)(SystemConfigChangeTime - SystemStartTime));

  if (!ra || cupsArrayFind(ra, "system-config-changes"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-config-changes", SystemConfigChanges);

  if (!ra || cupsArrayFind(ra, "system-configured-printers"))
  {
    size_t		i,		/* Looping var */
  			count;		/* Number of printers */
    ipp_t		*col;		/* Collection value */
    ipp_attribute_t	*printers;	/* system-configured-printers attribute */
    static const char * const types[] =	/* Service types */
    {
      "print",
      "print3d"
    };

    cupsRWLockRead(&PrintersRWLock);

    if ((count = cupsArrayGetCount(Printers)) == 0)
    {
      ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_NOVALUE, "system-configured-printers");
    }
    else
    {
      printers = ippAddCollections(client->response, IPP_TAG_SYSTEM, "system-configured-printers", count, NULL);

      for (i = 0, printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers), i ++)
      {
        cupsRWLockRead(&printer->rwlock);

        col = ippNew();
        ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_INTEGER, "printer-id", printer->id);
        ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "printer-info", NULL, printer->name);
        ippAddBoolean(col, IPP_TAG_ZERO, "printer-is-accepting-jobs", (char)ippGetBoolean(ippFindAttribute(printer->pinfo.attrs, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN), 0));
        ippAddString(col, IPP_TAG_ZERO, IPP_TAG_NAME, "printer-name", NULL, printer->name);
        ippAddString(col, IPP_TAG_ZERO, IPP_CONST_TAG(IPP_TAG_KEYWORD), "printer-service-type", NULL, types[printer->type]);
        ippAddInteger(col, IPP_TAG_ZERO, IPP_TAG_ENUM, "printer-state", (int)printer->state);
        serverCopyPrinterStateReasons(col, IPP_TAG_ZERO, printer);
        ippCopyAttribute(col, ippFindAttribute(printer->pinfo.attrs, "printer-xri-supported", IPP_TAG_BEGIN_COLLECTION), 1);

        ippSetCollection(client->response, &printers, i, col);
        ippDelete(col);

        cupsRWUnlock(&printer->rwlock);
      }
    }

    cupsRWUnlock(&PrintersRWLock);
  }

  /* TODO: Update when resources are implemented */
  if (!ra || cupsArrayFind(ra, "system-configured-resources"))
    ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_NOVALUE, "system-configured-resources");

  if (!ra || cupsArrayFind(ra, "system-current-time"))
    ippAddDate(client->response, IPP_TAG_SYSTEM, "system-current-time", ippTimeToDate(time(NULL)));

  if (!ra || cupsArrayFind(ra, "system-default-printer-id"))
  {
    if (DefaultPrinter)
      ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", DefaultPrinter->id);
    else
      ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_NOVALUE, "system-default-printer-id");
  }

  copy_system_state(client->response, ra);

  if (!ra || cupsArrayFind(ra, "system-up-time"))
    ippAddInteger(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - SystemStartTime));

#if 0 /* TODO: Add strings support for system object */
  if (printer->pinfo.strings && (!ra || cupsArrayFind(ra, "printer-strings-uri")))
  {
   /*
    * See if we have a localization that matches the request language.
    */

    ipp_attribute_t	*attr;		/* attributes-natural-language attribute */
    char		lang[32];	/* Copy of language string */
    server_lang_t	key, *match;	/* Localization key and match */

    ippFirstAttribute(client->request);
    attr = ippNextAttribute(client->request);
    cupsCopyString(lang, ippGetString(attr, 0, NULL), sizeof(lang));
    key.lang = lang;
    if ((match = cupsArrayFind(printer->pinfo.strings, &key)) == NULL && lang[2])
    {
     /*
      * Try base language...
      */

      lang[2] = '\0';
      match = cupsArrayFind(printer->pinfo.strings, &key);
    }

    if (match)
    {
      char		uri[1024];	/* printer-strings-uri value */
      server_listener_t	*lis = cupsArrayGetFirst(Listeners);
					/* Default listener */
      const char	*scheme = "http";
					/* URL scheme */

      if (Encryption != HTTP_ENCRYPTION_NEVER)
        scheme = "https";

      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), scheme, NULL, lis->host, lis->port, "%s/%s.strings", printer->resource, match->lang);
      ippAddString(client->response, IPP_TAG_PRINTER, IPP_TAG_URI, "printer-strings-uri", NULL, uri);
    }
  }
#endif /* 0 */

  cupsArrayDelete(ra);

  cupsRWUnlock(&SystemRWLock);
}


/*
 * 'ipp_get_system_supported_values()' - Get the supported values for the system object.
 */

static void
ipp_get_system_supported_values(
    server_client_t *client)		/* I - Client */
{
  cups_array_t		*ra;		/* Requested attributes array */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Send the attributes...
  */

  ra = ippCreateRequestedArray(client->request);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  /* system-default-printer-id (1setOf integer(1:65535)) */
  if (!ra || cupsArrayFind(ra, "system-default-printer-id"))
  {
    int			*values;	/* printer-id values */
    size_t		num_values,	/* Number of printer-id values */
  			count;		/* Number of printers */
    server_printer_t	*printer;	/* Current printer */

    cupsRWLockRead(&PrintersRWLock);

    if ((count = cupsArrayGetCount(Printers)) == 0)
    {
      ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_NOVALUE, "system-default-printer-id");
    }
    else if ((values = (int *)calloc((size_t)count, sizeof(int))) != NULL)
    {
      for (num_values = 0, printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
      {
        if (printer->id && printer->id <= 65535)
          values[num_values ++] = printer->id;
      }

      if (num_values > 0)
        ippAddIntegers(client->response, IPP_TAG_SYSTEM, IPP_TAG_INTEGER, "system-default-printer-id", num_values, values);
      else
	ippAddOutOfBand(client->response, IPP_TAG_SYSTEM, IPP_TAG_NOVALUE, "system-default-printer-id");

      free(values);
    }

    cupsRWUnlock(&PrintersRWLock);
  }

  cupsArrayDelete(ra);
}


/*
 * 'ipp_hold_job()' - Hold a pending job.
 */

static void
ipp_hold_job(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* Print job */
  ipp_attribute_t	*hold_until;	/* job-hold-until-xxx attribute, if any */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if ((hold_until = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    hold_until = ippFindAttribute(client->request, "job-hold-until-time", IPP_TAG_DATE);

  if (serverHoldJob(job, hold_until))
    serverRespondIPP(client, IPP_STATUS_OK, NULL);
  else
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Unable to hold job.");
}


/*
 * 'ipp_hold_new_jobs()' - Hold new jobs for printing.
 */

static void
ipp_hold_new_jobs(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Set the 'hold-new-jobs' reason...
  */

  cupsRWLockWrite(&client->printer->rwlock);

  client->printer->state_reasons |= SERVER_PREASON_HOLD_NEW_JOBS;

  cupsRWUnlock(&client->printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_identify_printer()' - Beep or display a message.
 */

static void
ipp_identify_printer(
    server_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*actions,	/* identify-actions */
			*message;	/* message */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  actions = ippFindAttribute(client->request, "identify-actions", IPP_TAG_KEYWORD);
  message = ippFindAttribute(client->request, "message", IPP_TAG_TEXT);

  if (client->printer->pinfo.proxy_group == SERVER_GROUP_NONE)
  {
   /*
    * Send a notification to the console...
    */

    if (ippContainsString(actions, "display"))
      printf("IDENTIFY-PRINTER: display (%s)\n", message ? ippGetString(message, 0, NULL) : "No message supplied");

    if (!actions || ippContainsString(actions, "sound"))
      puts("IDENTIFY-PRINTER: sound\007");
  }
  else
  {
   /*
    * Save this notification in the printer for the proxy to acknowledge...
    */

    cupsRWLockWrite(&client->printer->rwlock);

    client->printer->identify_actions = SERVER_IDENTIFY_NONE;
    if (ippContainsString(actions, "display"))
      client->printer->identify_actions |= SERVER_IDENTIFY_DISPLAY;
    if (!actions || ippContainsString(actions, "sound"))
      client->printer->identify_actions |= SERVER_IDENTIFY_SOUND;

    if (client->printer->identify_message)
    {
      free(client->printer->identify_message);
      client->printer->identify_message = NULL;
    }

    if (message)
      client->printer->identify_message = strdup(ippGetString(message, 0, NULL));

    client->printer->state_reasons |= SERVER_PREASON_IDENTIFY_PRINTER_REQUESTED;

    cupsRWUnlock(&client->printer->rwlock);

    serverAddEventNoLock(client->printer, NULL, NULL, SERVER_EVENT_PRINTER_STATE_CHANGED, "Identify-Printer request received.");
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_install_resource()' - Install a resource.
 */

static void
ipp_install_resource(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*attr;		/* Request attribute */
  int			resource_id;	/* resource-id value */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "resource-id", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-id' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (resource_id = ippGetInteger(attr, 0)) < 1)
  {
    serverRespondUnsupported(client, attr);
    return;
  }
  else if ((resource = serverFindResourceById(resource_id)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Resource #%d not found.", resource_id);
    return;
  }
  else if (resource->state != IPP_RSTATE_AVAILABLE)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Resource #%d not available.", resource_id);
    return;
  }

 /*
  * Set state to installed...
  */

  serverSetResourceState(resource, IPP_RSTATE_INSTALLED, NULL);

 /*
  * Send attributes...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = ippCreateRequestedArray(client->request);

  cupsRWLockRead(&resource->rwlock);
  copy_resource_attributes(client, resource, ra);
  cupsRWUnlock(&resource->rwlock);

  cupsArrayDelete(ra);
}


/*
 * 'ipp_pause_all_printers()' - Stop processing jobs for all printers.
 */

static void
ipp_pause_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&SystemRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    serverPausePrinter(printer, ippGetOperation(client->request) == IPP_OP_PAUSE_ALL_PRINTERS);

  cupsRWUnlock(&SystemRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_pause_printer()' - Stop processing jobs for a printer.
 */

static void
ipp_pause_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  serverPausePrinter(client->printer, ippGetOperation(client->request) == IPP_OP_PAUSE_PRINTER);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_print_job()' - Create a job object with an attached document.
 */

static void
ipp_print_job(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* New job */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*hold_until,	/* job-hold-until-xxx attribute, if any */
			*doc_name;	/* document-name attribute, if any */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if (!client->printer->is_accepting)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting jobs.");
    return;
  }

 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
    return;

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_SEND)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No file in request.");
    return;
  }

 /*
  * Print the job...
  */

  if ((job = serverCreateJob(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_TOO_MANY_JOBS, "Too many jobs are queued.");
    return;
  }

  if ((doc_name = ippFindAttribute(client->request, "document-name", IPP_TAG_NAME)) != NULL)
  {
    if (!job->doc_attrs)
      job->doc_attrs = ippNew();

    doc_name = ippCopyAttribute(job->doc_attrs, doc_name, 0);

    ippSetGroupTag(job->doc_attrs, &doc_name, IPP_TAG_DOCUMENT);
  }

  if ((hold_until = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    hold_until = ippFindAttribute(client->request, "job-hold-until-time", IPP_TAG_DATE);

  if (hold_until || (job->printer->state_reasons & SERVER_PREASON_HOLD_NEW_JOBS))
    serverHoldJob(job, hold_until);

 /*
  * Create a file for the request data...
  */

  serverCreateJobFilename(job, NULL, filename, sizeof(filename));

  serverLogJob(SERVER_LOGLEVEL_INFO, job, "Creating job file \"%s\", format \"%s\".", filename, job->format);

  if ((job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;		/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  job->fd       = -1;
  job->filename = strdup(filename);
  job->state    = IPP_JSTATE_PENDING;

 /*
  * Process the job, if possible...
  */

  serverCheckJobs(client->printer);

 /*
  * Return the job info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-message");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra, NULL);
  cupsArrayDelete(ra);

 /*
  * Process any pending subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_print_uri()' - Create a job object with a referenced document.
 */

static void
ipp_print_uri(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* New job */
  const char		*uri;		/* document-uri */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*hold_until,	/* job-hold-until-xxx attribute, if any */
			*doc_name;	/* document-name attribute, if any */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if (!client->printer->is_accepting)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_ACCEPTING_JOBS, "Not accepting jobs.");
    return;
  }

 /*
  * Validate print job attributes...
  */

  if (!valid_job_attributes(client))
    return;

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = get_document_uri(client)) == NULL)
    return;

 /*
  * Print the job...
  */

  if ((job = serverCreateJob(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_TOO_MANY_JOBS, "Too many jobs are queued.");
    return;
  }

  if ((doc_name = ippFindAttribute(client->request, "document-name", IPP_TAG_NAME)) != NULL)
  {
    if (!job->doc_attrs)
      job->doc_attrs = ippNew();

    doc_name = ippCopyAttribute(job->doc_attrs, doc_name, 0);

    ippSetGroupTag(job->doc_attrs, &doc_name, IPP_TAG_DOCUMENT);
  }

  if ((hold_until = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    hold_until = ippFindAttribute(client->request, "job-hold-until-time", IPP_TAG_DATE);

  if (hold_until || (job->printer->state_reasons & SERVER_PREASON_HOLD_NEW_JOBS))
    serverHoldJob(job, hold_until);

  if (copy_document_uri(client, job, uri) && job->hold_until == 0)
    job->state = IPP_JSTATE_PENDING;

 /*
  * Process the job...
  */

  if (job->state == IPP_JSTATE_PENDING)
    serverCheckJobs(client->printer);

 /*
  * Return the job info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra, NULL);
  cupsArrayDelete(ra);

 /*
  * Process any pending subscriptions...
  */

  client->job = job;
  ipp_create_xxx_subscriptions(client);
}


/*
 * 'ipp_register_output_device()' - Register an output device for proxying.
 */

static void
ipp_register_output_device(
    server_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*attr;		/* Attribute in request */
  const char		*uuid;		/* "output-device-uuid" value */
  server_printer_t	*printer,	/* Current printer */
			*avail = NULL;	/* Available printer */
  server_device_t	key,		/* Search key */
			*device;	/* Matching device */
  cups_array_t		*ra;		/* Response attributes */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthProxyGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "output-device-uuid", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'output-device-uuid' attribute.");
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The 'output-device-uuid' attribute is in the wrong group.");
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_URI || ippGetCount(attr) != 1 || (uuid = ippGetString(attr, 0, NULL)) == NULL || strncmp(uuid, "urn:uuid:", 9))
  {
    serverRespondUnsupported(client, attr);
    return;
  }

 /*
  * Look for a matching printer...
  */

  cupsRWLockRead(&PrintersRWLock);

  key.uuid = (char *)uuid;

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
  {
    if (printer->pinfo.proxy_group == SERVER_GROUP_NONE || printer->pinfo.max_devices == 0)
      continue;

    cupsRWLockRead(&printer->rwlock);
    device = (server_device_t *)cupsArrayFind(printer->pinfo.devices, &key);

    if (!avail && !device && cupsArrayGetCount(printer->pinfo.devices) < printer->pinfo.max_devices)
      avail = printer;

    cupsRWUnlock(&printer->rwlock);

    if (device)
      break;
  }

  cupsRWUnlock(&PrintersRWLock);

  if (!printer)
  {
    if (avail)
    {
     /*
      * Use available printer...
      */

      printer = client->printer = avail;
    }
    else
    {
     /*
      * No matching printer, so create one...
      */

      char		path[256];	/* Resource path */
      server_pinfo_t	pinfo;		/* Printer information */

      memset(&pinfo, 0, sizeof(pinfo));
      pinfo.attrs       = ippNew();
      pinfo.proxy_group = AuthProxyGroup;
      pinfo.max_devices = 1;

      exit(99);
      snprintf(path, sizeof(path), "/ipp/print/%s", uuid + 9);
      printer = client->printer = serverCreatePrinter(path, uuid + 9, uuid + 9, &pinfo, 0);

      serverAddPrinter(printer);
    }
  }

  serverCreateDevice(client);

  cupsRWLockRead(&client->printer->rwlock);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "printer-id");
  cupsArrayAdd(ra, "printer-is-accepting-jobs");
  cupsArrayAdd(ra, "printer-state");
  cupsArrayAdd(ra, "printer-state-reasons");
  cupsArrayAdd(ra, "printer-uuid");
  cupsArrayAdd(ra, "printer-xri-supported");
  cupsArrayAdd(ra, "system-state");
  cupsArrayAdd(ra, "system-state-reasons");

  serverCopyAttributes(client->response, printer->pinfo.attrs, ra, NULL, IPP_TAG_ZERO, IPP_TAG_ZERO);
  copy_printer_state(client->response, printer, ra);

  cupsRWUnlock(&client->printer->rwlock);
}


/*
 * 'ipp_release_held_new_jobs()' - Release any new jobs that were held.
 */

static void
ipp_release_held_new_jobs(
    server_client_t *client)		/* I - Client */
{
  server_job_t	*job;			/* Current job */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Clear the 'hold-new-jobs' reason and release any held jobs...
  */

  cupsRWLockWrite(&client->printer->rwlock);

  client->printer->state_reasons &= (server_preason_t)~SERVER_PREASON_HOLD_NEW_JOBS;

  for (job = (server_job_t *)cupsArrayGetFirst(client->printer->active_jobs); job; job = (server_job_t *)cupsArrayGetNext(client->printer->active_jobs))
  {
    if (job->state == IPP_JSTATE_HELD)
    {
      const char	*hold_until;	/* job-hold-until attribute, if any */
      int		resume;		/* Do we need to resume the job? */

      cupsRWLockRead(&job->rwlock);
      resume = (hold_until = ippGetString(ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO), 0, NULL)) != NULL && !strcmp(hold_until, "none");
      cupsRWUnlock(&job->rwlock);

      if (resume)
        serverReleaseJob(job);
    }
  }

  cupsRWUnlock(&client->printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_release_job()' - Release a held job.
 */

static void
ipp_release_job(server_client_t *client)	/* I - Client */
{
  server_job_t	*job;				/* Print job */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if (serverReleaseJob(job))
    serverRespondIPP(client, IPP_STATUS_OK, NULL);
  else
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Unable to release job.");

  serverCheckJobs(client->printer);
}


/*
 * 'ipp_renew_subscription()' - Renew a subscription.
 */

static void
ipp_renew_subscription(
    server_client_t *client)		/* I - Client */
{
  server_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*attr;		/* notify-lease-duration */
  int			lease;		/* Lease duration in seconds */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if ((sub = serverFindSubscription(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription was not found.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, sub->username, SERVER_GROUP_NONE, SubscriptionPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this subscription.");
    return;
  }

  if (sub->job)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Per-job subscriptions cannot be renewed.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "notify-lease-duration", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) < 0)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad notify-lease-duration.");
      return;
    }

    lease = ippGetInteger(attr, 0);
  }
  else
    lease = SERVER_NOTIFY_LEASE_DURATION_DEFAULT;

  sub->lease = lease;

  if (lease)
    sub->expire = time(NULL) + sub->lease;
  else
    sub->expire = INT_MAX;

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-duration", (int)(sub->expire - time(NULL)));
}


/*
 * 'ipp_restart_printer()' - Restart a printer.
 */

static void
ipp_restart_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  serverRestartPrinter(client->printer);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_restart_system()' - Restart all printers.
 */

static void
ipp_restart_system(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  /* TODO: Actually do a full restart of the system... */
  serverSaveSystem();

  cupsRWLockRead(&SystemRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    serverRestartPrinter(printer);

  cupsRWUnlock(&SystemRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_resume_all_printers()' - Start processing jobs for all printers.
 */

static void
ipp_resume_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&SystemRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
    serverResumePrinter(printer);

  cupsRWUnlock(&SystemRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_resume_printer()' - Start processing jobs for a printer.
 */

static void
ipp_resume_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  serverResumePrinter(client->printer);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_send_document()' - Add an attached document to a job object created with
 *                         Create-Job.
 */

static void
ipp_send_document(server_client_t *client)/* I - Client */
{
  server_job_t		*job;		/* Job information */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    httpFlush(client->http);
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    serverRespondUnsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  if (!job->doc_attrs)
    job->doc_attrs = ippNew();

  serverCopyAttributes(job->doc_attrs, client->request, NULL, NULL, IPP_TAG_JOB, 0);

 /*
  * Get the document format for the job...
  */

  cupsRWLockWrite(&(client->printer->rwlock));

  if ((attr = ippFindAttribute(job->attrs, "document-format-detected", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else if ((attr = ippFindAttribute(job->attrs, "document-format", IPP_TAG_MIMETYPE)) != NULL)
    job->format = ippGetString(attr, 0, NULL);
  else
    job->format = "application/octet-stream";

 /*
  * Create a file for the request data...
  */

  serverCreateJobFilename(job, NULL, filename, sizeof(filename));

  serverLogJob(SERVER_LOGLEVEL_INFO, job, "Creating job file \"%s\", format \"%s\".", filename, job->format);

  job->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);

  cupsRWUnlock(&(client->printer->rwlock));

  if (job->fd < 0)
  {
    job->state = IPP_JSTATE_ABORTED;

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to create print file: %s", strerror(errno));
    return;
  }

  while ((bytes = httpRead(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(job->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      job->state = IPP_JSTATE_ABORTED;

      close(job->fd);
      job->fd = -1;

      unlink(filename);

      serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                  "Unable to write print file: %s", strerror(error));
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the print data, so abort this job.
    */

    job->state = IPP_JSTATE_ABORTED;

    close(job->fd);
    job->fd = -1;

    unlink(filename);

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to read print file.");
    return;
  }

  if (close(job->fd))
  {
    int error = errno;			/* Write error */

    job->state = IPP_JSTATE_ABORTED;
    job->fd    = -1;

    unlink(filename);

    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL,
                "Unable to write print file: %s", strerror(error));
    return;
  }

  cupsRWLockWrite(&(client->printer->rwlock));

  job->fd       = -1;
  job->filename = strdup(filename);

  if (job->hold_until == 0)
    job->state = IPP_JSTATE_PENDING;

  cupsRWUnlock(&(client->printer->rwlock));

 /*
  * Process the job, if possible...
  */

  serverCheckJobs(client->printer);

 /*
  * Return the job info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra, NULL);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_send_resource_data()' - Receive data for a resource.
 */

static void
ipp_send_resource_data(
    server_client_t *client)		/* I - Client */
{
  server_resource_t	*resource;	/* New resource */
  cups_array_t		*ra;		/* Attributes to send in response */
  ipp_attribute_t	*attr;		/* Request attribute */
  int			resource_id;	/* resource-id value */
  const char		*format;	/* resource-format value */
  ipp_attribute_t	*signature;	/* resource-signature value */
  char			filename[1024],	/* Filename buffer */
			buffer[4096];	/* Copy buffer */
  ssize_t		bytes;		/* Bytes read */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Validate request attributes...
  */

  if ((attr = ippFindAttribute(client->request, "resource-id", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-id' attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (resource_id = ippGetInteger(attr, 0)) < 1)
  {
    serverRespondUnsupported(client, attr);
    httpFlush(client->http);
    return;
  }
  else if ((resource = serverFindResourceById(resource_id)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Resource #%d not found.", resource_id);
    httpFlush(client->http);
    return;
  }
  else if (resource->state != IPP_RSTATE_PENDING)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Resource #%d is not in the pending state.", resource_id);
    httpFlush(client->http);
    return;
  }
  else if (resource->fd >= 0)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Resource #%d is already incoming.", resource_id);
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "resource-format", IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required 'resource-format' attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_MIMETYPE || ippGetCount(attr) != 1 || (format = ippGetString(attr, 0, NULL)) == NULL || (strcmp(format, "application/ipp") && strcmp(format, "application/pdf") && strcmp(format, "application/vnd.iccprofile") && strcmp(format, "image/jpeg") && strcmp(format, "image/png") && strcmp(format, "text/strings")))
  {
    serverRespondUnsupported(client, attr);
    httpFlush(client->http);
    return;
  }

  if ((signature = ippFindAttribute(client->request, "resource-signature", IPP_TAG_ZERO)) != NULL && (ippGetGroupTag(signature) != IPP_TAG_OPERATION || ippGetValueTag(signature) != IPP_TAG_STRING))
  {
    serverRespondUnsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Copy the remaining message body to the resource file...
  */

  serverCreateResourceFilename(resource, format, SpoolDirectory, filename, sizeof(filename));

  serverLogClient(SERVER_LOGLEVEL_INFO, client, "Creating resource file \"%s\", format \"%s\".", filename, format);

  if ((resource->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0)
  {
    int error = errno;			/* Open error */

    serverSetResourceState(resource, IPP_RSTATE_ABORTED, "Unable to create resource file: %s", strerror(error));
    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to create resource file: %s", strerror(error));
    httpFlush(client->http);
    return;
  }

  while ((bytes = httpRead(client->http, buffer, sizeof(buffer))) > 0)
  {
    if (write(resource->fd, buffer, (size_t)bytes) < bytes)
    {
      int error = errno;		/* Write error */

      close(resource->fd);
      resource->fd = -1;
      unlink(filename);

      serverSetResourceState(resource, IPP_RSTATE_ABORTED, "Unable to write resource file: %s", strerror(error));
      serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write resource file: %s", strerror(error));
      httpFlush(client->http);
      return;
    }
  }

  if (bytes < 0)
  {
   /*
    * Got an error while reading the resource data, so abort this resource.
    */

    close(resource->fd);
    resource->fd = -1;
    unlink(filename);

    serverSetResourceState(resource, IPP_RSTATE_ABORTED, "Unable to read resource file.");
    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to read resource file.");
    return;
  }

  if (close(resource->fd))
  {
    int error = errno;			/* Write error */

    resource->fd = -1;
    unlink(filename);

    serverSetResourceState(resource, IPP_RSTATE_ABORTED, "Unable to write resource file: %s", strerror(error));
    serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to write resource file: %s", strerror(error));
    return;
  }

  resource->fd = -1;

  serverAddResourceFile(resource, filename, format);

  if (signature)
  {
    cupsRWLockWrite(&(resource->rwlock));

    if ((attr = ippCopyAttribute(resource->attrs, signature, 0)) != NULL)
      ippSetGroupTag(resource->attrs, &attr, IPP_TAG_RESOURCE);

    cupsRWUnlock(&(resource->rwlock));
  }

 /*
  * Return the resource info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "resource-id");
  cupsArrayAdd(ra, "resource-state");
  cupsArrayAdd(ra, "resource-state-reasons");
  cupsArrayAdd(ra, "resource-uuid");

  copy_resource_attributes(client, resource, ra);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_send_uri()' - Add a referenced document to a job object created with
 *                    Create-Job.
 */

static void
ipp_send_uri(server_client_t *client)	/* I - Client */
{
  server_job_t		*job;		/* Job information */
  const char		*uri;		/* document-uri */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_array_t		*ra;		/* Attributes to send in response */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    httpFlush(client->http);
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

 /*
  * See if we already have a document for this job or the job has already
  * in a non-pending state...
  */

  if (job->state > IPP_JSTATE_HELD)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE,
                "Job is not in a pending state.");
    httpFlush(client->http);
    return;
  }
  else if (job->filename || job->fd >= 0)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_MULTIPLE_JOBS_NOT_SUPPORTED,
                "Multiple document jobs are not supported.");
    httpFlush(client->http);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "last-document",
                               IPP_TAG_ZERO)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Missing required last-document attribute.");
    httpFlush(client->http);
    return;
  }
  else if (ippGetValueTag(attr) != IPP_TAG_BOOLEAN || ippGetCount(attr) != 1 ||
           !ippGetBoolean(attr, 0))
  {
    serverRespondUnsupported(client, attr);
    httpFlush(client->http);
    return;
  }

 /*
  * Validate document attributes...
  */

  if (!valid_doc_attributes(client))
  {
    httpFlush(client->http);
    return;
  }

  if (!job->doc_attrs)
    job->doc_attrs = ippNew();

  serverCopyAttributes(job->doc_attrs, client->request, NULL, NULL, IPP_TAG_JOB, 0);

 /*
  * Do we have a file to print?
  */

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST,
                "Unexpected document data following request.");
    return;
  }

 /*
  * Do we have a document URI?
  */

  if ((uri = get_document_uri(client)) == NULL)
    return;

 /*
  * Get the document format for the job...
  */

  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_MIMETYPE)) != NULL)
  {
    cupsRWLockWrite(&job->rwlock);

    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, ippGetString(attr, 0, NULL));

    job->format = ippGetString(attr, 0, NULL);

    cupsRWUnlock(&job->rwlock);
  }
  else
    job->format = "application/octet-stream";

  if (copy_document_uri(client, job, uri) && job->hold_until == 0)
    job->state = IPP_JSTATE_PENDING;

 /*
  * Process the job, if possible...
  */

  if (job->state == IPP_JSTATE_PENDING)
    serverCheckJobs(client->printer);

 /*
  * Return the job info...
  */

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  ra = cupsArrayNew((cups_array_cb_t)strcmp, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(ra, "job-id");
  cupsArrayAdd(ra, "job-state");
  cupsArrayAdd(ra, "job-state-reasons");
  cupsArrayAdd(ra, "job-uri");

  copy_job_attributes(client, job, ra, NULL);
  cupsArrayDelete(ra);
}


/*
 * 'ipp_set_document_attributes()' - Set document attributes.
 */

static void
ipp_set_document_attributes(
    server_client_t *client)		/* I - Client */
{
  server_job_t		*job;		/* Job information */
  const char		*name;		/* Name of attribute */
  ipp_attribute_t	*attr;		/* Current attribute */
  int			doc_number;	/* Document number value */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad 'document-number' attribute in request." : "Missing 'document-number' attribute in request.");
    return;
  }
  else if ((doc_number = ippGetInteger(attr, 0)) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Document #%d does not exist.", doc_number);
    return;
  }

  if (job->state >= IPP_JSTATE_PROCESSING)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending/pending-held state.");
    return;
  }

 /*
  * Scan attributes to see if there are any that are not settable...
  */

  if (!valid_doc_attributes(client))
    return;

 /*
  * Set the values...
  */

  cupsRWLockWrite(&job->rwlock);

  for (attr = ippFirstAttribute(client->request); attr; attr = ippNextAttribute(client->request))
  {
    ipp_attribute_t	*old_attr;	/* Old attribute value */

    name = ippGetName(attr);

    if (ippGetGroupTag(attr) != IPP_TAG_DOCUMENT || !name)
      continue;

    if ((old_attr = ippFindAttribute(job->doc_attrs, name, IPP_TAG_ZERO)) != NULL)
      ippDeleteAttribute(job->doc_attrs, old_attr);

    if (!job->doc_attrs)
      job->doc_attrs = ippNew();

    ippCopyAttribute(job->doc_attrs, attr, 0);
  }

  cupsRWUnlock(&job->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_set_job_attributes()' - Set job attributes.
 */

static void
ipp_set_job_attributes(
    server_client_t *client)		/* I - Client */
{
  server_job_t		*job;		/* Job information */
  const char		*name;		/* Name of attribute */
  ipp_attribute_t	*attr;		/* Current attribute */


  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

 /*
  * Get the job...
  */

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job does not exist.");
    return;
  }

  if (Authentication && !serverAuthorizeUser(client, job->username, SERVER_GROUP_NONE, JobPrivacyScope))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this job.");
    return;
  }

  if (job->state >= IPP_JSTATE_PROCESSING)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job is not in a pending/pending-held state.");
    return;
  }

 /*
  * Scan attributes to see if there are any that are not settable...
  */

  if (!valid_job_attributes(client))
    return;

 /*
  * Set the values...
  */

  for (attr = ippFirstAttribute(client->request); attr; attr = ippNextAttribute(client->request))
  {
    name = ippGetName(attr);

    if (ippGetGroupTag(attr) != IPP_TAG_JOB || !name)
      continue;

    if (!strcmp(name, "job-hold-until"))
    {
      const char *value = ippGetString(attr, 0, NULL);
					/* job-hold-until value */

      if (!strcmp(value, "no-hold"))
        serverReleaseJob(job);
      else
        serverHoldJob(job, attr);
    }
    else if (!strcmp(name, "job-hold-until-time"))
    {
      serverHoldJob(job, attr);
    }
    else if (!strcmp(name, "job-priority"))
    {
      cupsRWLockWrite(&job->printer->rwlock);

      cupsArrayRemove(job->printer->active_jobs, job);

      job->priority = ippGetInteger(attr, 0);

      cupsArrayAdd(job->printer->active_jobs, job);

      cupsRWUnlock(&job->printer->rwlock);
    }
    else
    {
      ipp_attribute_t	*old_attr;	/* Old attribute value */

      cupsRWLockWrite(&job->rwlock);

      if ((old_attr = ippFindAttribute(job->attrs, name, IPP_TAG_ZERO)) != NULL)
        ippDeleteAttribute(job->attrs, old_attr);

      ippCopyAttribute(job->attrs, attr, 0);

      cupsRWUnlock(&job->rwlock);
    }
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_set_printer_attributes()' - Set printer attributes.
 */

static void
ipp_set_printer_attributes(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Printer */
  const char		*name;		/* Name of attribute */
  size_t		count;		/* Number of values */
  ipp_attribute_t	*attr,		/* Current attribute */
			*settable;	/* Settable values */
  const char		*value;		/* Attribute value */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Scan attributes to see if there are any that are not settable...
  */

  printer = client->printer;

  cupsRWLockWrite(&printer->rwlock);

  settable = ippFindAttribute(printer->pinfo.attrs, "printer-settable-attributes-supported", IPP_TAG_KEYWORD);

  if (!valid_values(client, IPP_TAG_PRINTER, settable, sizeof(printer_values) / sizeof(printer_values[0]), printer_values))
  {
    cupsRWUnlock(&printer->rwlock);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-geo-location", IPP_TAG_URI)) != NULL && ((value = ippGetString(attr, 0, NULL)) == NULL || strncmp(value, "geo:", 4)))
  {
    serverRespondUnsupported(client, attr);
    cupsRWUnlock(&printer->rwlock);
    return;
  }

  if ((attr = ippFindAttribute(client->request, "printer-icc-profiles", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    size_t		i;		/* Looping var */
    bool		bad_col = false;/* Bad collection value? */
    ipp_t		*col;		/* Collection value */
    ipp_attribute_t	*colattr;	/* Collection attribute */
    const char		*colname;	/* Collection attribute name */
    ipp_tag_t		coltag;		/* Collection attribute value type */

    for (i = 0, count = ippGetCount(attr); i < count && !bad_col; i ++)
    {
      bool	saw_name = false,	/* Saw profile-name? */
		saw_uri = false;	/* Saw profile-uri? */

      col = ippGetCollection(attr, i);

      for (colattr = ippFirstAttribute(col); colattr; colattr = ippNextAttribute(col))
      {
	colname = ippGetName(colattr);
	coltag  = ippGetValueTag(colattr);

	if (!colname || !ippValidateAttribute(colattr))
	{
	  bad_col = true;
	}
	else if (!strcmp(colname, "profile-name"))
	{
	  if ((coltag != IPP_TAG_NAME && coltag != IPP_TAG_NAMELANG) || ippGetCount(colattr) != 1)
	    bad_col = true;
	  else
	    saw_name = true;
	}
	else if (!strcmp(colname, "profile-uri"))
	{
	  if (coltag != IPP_TAG_URI || ippGetCount(colattr) != 1)
	    bad_col = true;
	  else
	    saw_uri = true;
	}
      }

      if (!saw_name || !saw_uri)
	bad_col = true;
    }

    if (bad_col)
    {
      serverRespondUnsupported(client, attr);
      cupsRWUnlock(&printer->rwlock);
      return;
    }
  }

 /*
  * Set the values...
  */

  for (attr = ippFirstAttribute(client->request); attr; attr = ippNextAttribute(client->request))
  {
    ipp_attribute_t	*old_attr;	/* Old attribute */

    name = ippGetName(attr);

    if (ippGetGroupTag(attr) != IPP_TAG_PRINTER || !name)
      continue;

    value    = ippGetString(attr, 0, NULL);
    old_attr = ippFindAttribute(printer->pinfo.attrs, name, IPP_TAG_ZERO);

    if (!strcmp(name, "printer-dns-sd-name"))
    {
      serverUnregisterPrinter(printer);

      if (printer->dns_sd_name)
      {
        free(printer->dns_sd_name);
        printer->dns_sd_name = NULL;
      }

      if (value)
        printer->dns_sd_name = strdup(value);

      serverRegisterPrinter(printer);
    }
    else if (!strcmp(name, "printer-geo-location"))
    {
      serverUnregisterPrinter(printer);

      if (old_attr)
        ippDeleteAttribute(printer->pinfo.attrs, old_attr);

      ippCopyAttribute(printer->pinfo.attrs, attr, 0);

      serverRegisterPrinter(printer);
    }
    else if (!strcmp(name, "printer-name"))
    {
      if (printer->name)
        free(printer->name);

      printer->name = strdup(value);

      if (old_attr)
        ippDeleteAttribute(printer->pinfo.attrs, old_attr);

      ippCopyAttribute(printer->pinfo.attrs, attr, 0);
    }
    else
    {
      if (old_attr)
        ippDeleteAttribute(printer->pinfo.attrs, old_attr);

      ippCopyAttribute(printer->pinfo.attrs, attr, 0);
    }
  }

  cupsRWUnlock(&printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_set_resource_attributes()' - Set resource attributes.
 */

static void
ipp_set_resource_attributes(
    server_client_t *client)		/* I - Client */
{
  serverRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "This operation is not yet implemented.");
  return;
}


/*
 * 'ipp_set_system_attributes()' - Set attributes for the system object.
 */

static void
ipp_set_system_attributes(
    server_client_t *client)		/* I - Client */
{
  ipp_attribute_t	*settable,	/* system-settable-attributes-supported */
			*attr;		/* Current attribute */
  static server_value_t	values[] =	/* Value tags for settable attributes */
  {
    { "system-default-printer-id",	IPP_TAG_INTEGER, IPP_TAG_NOVALUE, 0 },
    { "system-geo-location",		IPP_TAG_URI, IPP_TAG_UNKNOWN, 0 },
    { "system-info",			IPP_TAG_TEXT, IPP_TAG_ZERO, 0 },
    { "system-location",		IPP_TAG_TEXT, IPP_TAG_ZERO, 0 },
    { "system-make-and-model",		IPP_TAG_TEXT, IPP_TAG_ZERO, 0 },
    { "system-name",			IPP_TAG_NAME, IPP_TAG_ZERO, 0 },
    { "system-owner-col",		IPP_TAG_BEGIN_COLLECTION, IPP_TAG_NOVALUE, 0 }
  };


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockWrite(&SystemRWLock);

 /*
  * Validate request before setting attributes so that the Set operation is
  * atomic...
  */

  settable = ippFindAttribute(SystemAttributes, "system-settable-attributes-supported", IPP_TAG_KEYWORD);

  if (!valid_values(client, IPP_TAG_SYSTEM, settable, sizeof(values) / sizeof(values[0]), values))
    goto unlock_system;

  if ((attr = ippFindAttribute(client->request, "system-owner-col", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    ipp_t		*col = ippGetCollection(attr, 0);
					/* Collection value */
    ipp_attribute_t	*member;	/* Member attribute */

    for (member = ippFirstAttribute(col); member; member = ippNextAttribute(col))
    {
      const char *mname = ippGetName(member);

      if (strcmp(mname, "owner-uri") && strcmp(mname, "owner-name") && strcmp(mname, "owner-vcard"))
      {
        serverRespondUnsupported(client, attr);
	goto unlock_system;
      }
      else if ((!strcmp(mname, "owner-uri") && (ippGetValueTag(member) != IPP_TAG_URI || ippGetCount(member) != 1)) ||
	       (!strcmp(mname, "owner-name") && ((ippGetValueTag(member) != IPP_TAG_NAME && ippGetValueTag(member) != IPP_TAG_NAMELANG) || ippGetCount(member) != 1)) ||
	       (!strcmp(mname, "owner-vcard") && ippGetValueTag(member) != IPP_TAG_TEXT && ippGetValueTag(member) != IPP_TAG_TEXTLANG))
      {
        serverRespondUnsupported(client, attr);
	goto unlock_system;
      }
    }
  }

  for (attr = ippFirstAttribute(client->request); attr; attr = ippNextAttribute(client->request))
  {
    const char		*name = ippGetName(attr);
					/* Attribute name */
    ipp_attribute_t	*sattr;		/* Attribute to set */

    if (!name || ippGetGroupTag(attr) != IPP_TAG_SYSTEM)
      continue;

    if ((sattr = ippFindAttribute(SystemAttributes, name, IPP_TAG_ZERO)) != NULL)
    {
      switch (ippGetValueTag(attr))
      {
        case IPP_TAG_INTEGER :
            ippSetInteger(SystemAttributes, &sattr, 0, ippGetInteger(attr, 0));
            break;

        case IPP_TAG_NAME :
        case IPP_TAG_NAMELANG :
        case IPP_TAG_TEXT :
        case IPP_TAG_TEXTLANG :
           /*
            * Need to copy since ippSetString doesn't handle setting the
            * language override.
            */

	    ippDeleteAttribute(SystemAttributes, sattr);
	    ippCopyAttribute(SystemAttributes, attr, 0);
            break;

        case IPP_TAG_URI :
            ippSetString(SystemAttributes, &sattr, 0, ippGetString(attr, 0, NULL));
            break;

        case IPP_TAG_BEGIN_COLLECTION :
            ippSetCollection(SystemAttributes, &sattr, 0, ippGetCollection(attr, 0));
            break;

        default :
            break;
      }
    }
  }

 /*
  * Update config change time and count...
  */

  SystemConfigChangeTime = time(NULL);
  SystemConfigChanges ++;

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  unlock_system:

  cupsRWUnlock(&SystemRWLock);
}


/*
 * 'ipp_shutdown_all_printers()' - Shutdown all printers.
 */

static void
ipp_shutdown_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&PrintersRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
  {
    cupsRWLockWrite(&printer->rwlock);

    printer->is_shutdown = 1;
    printer->state_reasons |= SERVER_PREASON_PRINTER_SHUTDOWN;

    if (printer->processing_job)
      serverStopJob(printer->processing_job);
    else
      printer->state = IPP_PSTATE_STOPPED;

    cupsRWUnlock(&printer->rwlock);
  }

  cupsRWUnlock(&PrintersRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_shutdown_printer()' - Shutdown a printer.
 */

static void
ipp_shutdown_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockWrite(&client->printer->rwlock);

  client->printer->is_shutdown = 1;
  client->printer->state_reasons |= SERVER_PREASON_PRINTER_SHUTDOWN;

  if (client->printer->processing_job)
    serverStopJob(client->printer->processing_job);
  else
    client->printer->state = IPP_PSTATE_STOPPED;

  cupsRWUnlock(&client->printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_startup_all_printers()' - Start all printers.
 */

static void
ipp_startup_all_printers(
    server_client_t *client)		/* I - Client */
{
  server_printer_t	*printer;	/* Current printer */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockRead(&PrintersRWLock);

  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
  {
    cupsRWLockWrite(&printer->rwlock);

    if (printer->is_shutdown)
    {
      printer->is_shutdown = 0;
      printer->state_reasons &= (server_preason_t)~SERVER_PREASON_PRINTER_SHUTDOWN;
    }
    else
    {
      printer->is_accepting = 1;

      if (printer->processing_job)
      {
	serverStopJob(printer->processing_job);
      }
      else if (printer->state == IPP_PSTATE_STOPPED)
      {
	printer->state         = IPP_PSTATE_IDLE;
	printer->state_reasons = SERVER_PREASON_NONE;

	serverCheckJobs(printer);
      }
    }

    cupsRWUnlock(&printer->rwlock);
  }

  cupsRWUnlock(&PrintersRWLock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_startup_printer()' - Start a printer.
 */

static void
ipp_startup_printer(
    server_client_t *client)		/* I - Client */
{
  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the admin group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, AuthAdminGroup, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  cupsRWLockWrite(&client->printer->rwlock);

  if (client->printer->is_shutdown)
  {
    client->printer->is_shutdown = 0;
    client->printer->state_reasons &= (server_preason_t)~SERVER_PREASON_PRINTER_SHUTDOWN;
  }
  else
  {
    client->printer->is_accepting = 1;

    if (client->printer->processing_job)
    {
      serverStopJob(client->printer->processing_job);
    }
    else if (client->printer->state == IPP_PSTATE_STOPPED)
    {
      client->printer->state         = IPP_PSTATE_IDLE;
      client->printer->state_reasons = SERVER_PREASON_NONE;

      serverCheckJobs(client->printer);
    }
  }

  cupsRWUnlock(&client->printer->rwlock);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_update_active_jobs()' - Update the list of active jobs.
 */

static void
ipp_update_active_jobs(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Output device */
  server_job_t		*job;		/* Job */
  ipp_attribute_t	*job_ids,	/* job-ids */
			*job_states;	/* output-device-job-states */
  size_t		i,		/* Looping var */
			count,		/* Number of values */
			num_different = 0,
					/* Number of jobs with different states */
			num_unsupported = 0;
					/* Number of unsupported job-ids */
  int			different[1000],/* Jobs with different states */
			unsupported[1000];
					/* Unsupported job-ids */
  ipp_jstate_t		states[1000];	/* Different job state values */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

 /*
  * Process the job-ids and output-device-job-states values...
  */

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job_ids = ippFindAttribute(client->request, "job-ids", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(job_ids) != IPP_TAG_OPERATION || ippGetValueTag(job_ids) != IPP_TAG_INTEGER)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, job_ids ? "Bad job-ids attribute." : "Missing required job-ids attribute.");
    return;
  }

  if ((job_states = ippFindAttribute(client->request, "output-device-job-states", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(job_states) != IPP_TAG_OPERATION || ippGetValueTag(job_states) != IPP_TAG_ENUM)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, job_ids ? "Bad output-device-job-states attribute." : "Missing required output-device-job-states attribute.");
    return;
  }

  count = ippGetCount(job_ids);
  if (count != ippGetCount(job_states))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The job-ids and output-device-job-states attributes do not have the same number of values.");
    return;
  }

  for (i = 0; i < count; i ++)
  {
    if ((job = serverFindJob(client, ippGetInteger(job_ids, i))) == NULL || !job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
    {
      if (num_unsupported < 1000)
        unsupported[num_unsupported ++] = ippGetInteger(job_ids, i);
    }
    else
    {
      ipp_jstate_t	state = (ipp_jstate_t)ippGetInteger(job_states, i);

      if (job->state >= IPP_JSTATE_STOPPED && state != job->state)
      {
        if (num_different < 1000)
	{
	  different[num_different] = job->id;
	  states[num_different ++] = job->state;
	}
      }
      else
        job->dev_state = state;
    }
  }

 /*
  * Then look for jobs assigned to the device but not listed...
  */

  for (job = (server_job_t *)cupsArrayGetFirst(client->printer->jobs);
       job && num_different < 1000;
       job = (server_job_t *)cupsArrayGetNext(client->printer->jobs))
  {
    if (job->dev_uuid && !strcmp(job->dev_uuid, device->uuid) && !ippContainsInteger(job_ids, job->id))
    {
      different[num_different] = job->id;
      states[num_different ++] = job->state;
    }
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);

  if (num_different > 0)
  {
    ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-ids", num_different, different);
    ippAddIntegers(client->response, IPP_TAG_OPERATION, IPP_TAG_ENUM, "output-device-job-states", num_different, (int *)states);
  }

  if (num_unsupported > 0)
  {
    ippAddIntegers(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "job-ids", num_unsupported, unsupported);
  }
}


/*
 * 'ipp_update_document_status()' - Update the state of a document.
 */

static void
ipp_update_document_status(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "document-number", IPP_TAG_ZERO)) == NULL || ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || ippGetInteger(attr, 0) != 1)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, attr ? "Bad document-number attribute." : "Missing document-number attribute.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "impressions-completed", IPP_TAG_INTEGER)) != NULL)
  {
    job->impcompleted = ippGetInteger(attr, 0);
    serverAddEventNoLock(client->printer, job, NULL, SERVER_EVENT_JOB_PROGRESS, NULL);
  }

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_update_job_status()' - Update the state of a job.
 */

static void
ipp_update_job_status(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  server_job_t		*job;		/* Job */
  ipp_attribute_t	*attr;		/* Attribute */
  server_event_t		events = SERVER_EVENT_NONE;
					/* Event(s) */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Device was not found.");
    return;
  }

  if ((job = serverFindJob(client, 0)) == NULL)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job was not found.");
    return;
  }

  if (!job->dev_uuid || strcmp(job->dev_uuid, device->uuid))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Job not assigned to device.");
    return;
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions-completed", IPP_TAG_INTEGER)) != NULL)
  {
    job->impcompleted = ippGetInteger(attr, 0);
    events |= SERVER_EVENT_JOB_PROGRESS;
  }

  if ((attr = ippFindAttribute(client->request, "output-device-job-state", IPP_TAG_ENUM)) != NULL)
  {
    job->dev_state = (ipp_jstate_t)ippGetInteger(attr, 0);
    events |= SERVER_EVENT_JOB_STATE_CHANGED;
  }

  if ((attr = ippFindAttribute(client->request, "output-device-job-state-reasons", IPP_TAG_KEYWORD)) != NULL)
  {
    job->dev_state_reasons = serverGetJobStateReasonsBits(attr);
    events |= SERVER_EVENT_JOB_STATE_CHANGED;
  }

  if (events)
    serverAddEventNoLock(client->printer, job, NULL, events, NULL);

  serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_update_output_device_attributes()' - Update the values for an output device.
 */

static void
ipp_update_output_device_attributes(
    server_client_t *client)		/* I - Client */
{
  server_device_t	*device;	/* Device */
  ipp_attribute_t	*attr,		/* Current attribute */
			*dev_attr;	/* Device attribute */
  server_event_t	events = SERVER_EVENT_NONE;
					/* Config/state changed? */


  if (Authentication)
  {
   /*
    * Require authenticated username belonging to the proxy group...
    */

    if (!client->username[0])
    {
      serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
      return;
    }

    if (!serverAuthorizeUser(client, NULL, client->printer->pinfo.proxy_group, SERVER_SCOPE_DEFAULT))
    {
      serverRespondHTTP(client, HTTP_STATUS_FORBIDDEN, NULL, NULL, 0);
      return;
    }
  }

  if ((device = serverFindDevice(client)) == NULL)
  {
    if ((device = serverCreateDevice(client)) == NULL)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Unable to add output device.");
      return;
    }
  }

  cupsRWLockWrite(&device->rwlock);

  attr = ippFirstAttribute(client->request);
  while (attr && ippGetGroupTag(attr) != IPP_TAG_PRINTER)
    attr = ippNextAttribute(client->request);

  for (; attr; attr = ippNextAttribute(client->request))
  {
    const char	*attrname = ippGetName(attr),
					/* Attribute name */
		*dotptr;		/* Pointer to dot in name */

   /*
    * Skip attributes we don't care about...
    */

    if (!attrname)
      continue;

    if (strncmp(attrname, "copies", 6) && strncmp(attrname, "document-format", 15) && strncmp(attrname, "finishings", 10) && strncmp(attrname, "media", 5) && strncmp(attrname, "print-", 6) && strncmp(attrname, "sides", 5) && strncmp(attrname, "printer-alert", 13) && strncmp(attrname, "printer-input", 13) && strncmp(attrname, "printer-output", 14) && strncmp(attrname, "printer-resolution", 18) && strncmp(attrname, "pwg-raster", 10) && strncmp(attrname, "urf-", 4))
      continue;

    if (strncmp(attrname, "printer-alert", 13) || strncmp(attrname, "printer-state", 13))
      events |= SERVER_EVENT_PRINTER_CONFIG_CHANGED;
    else
      events |= SERVER_EVENT_PRINTER_STATE_CHANGED;

    if (!strcmp(attrname, "media-col-ready") || !strcmp(attrname, "media-ready"))
      events |= SERVER_EVENT_PRINTER_MEDIA_CHANGED;

    if (!strcmp(attrname, "finishings-col-ready") || !strcmp(attrname, "finishings-ready"))
      events |= SERVER_EVENT_PRINTER_FINISHINGS_CHANGED;

    if ((dotptr = strrchr(attrname, '.')) != NULL && isdigit(dotptr[1] & 255))
    {
     /*
      * Sparse representation: name.NNN or name.NNN-NNN
      */

      char	temp[256],		/* Temporary name string */
		*tempptr;		/* Pointer into temporary string */
      size_t	low, high;		/* Low and high numbers in range */

      low = (size_t)strtol(dotptr + 1, (char **)&dotptr, 10);
      if (dotptr && *dotptr == '-')
        high = (size_t)strtol(dotptr + 1, NULL, 10);
      else
        high = low;

      cupsCopyString(temp, attrname, sizeof(temp));
      if ((tempptr = strrchr(temp, '.')) != NULL)
        *tempptr = '\0';

      if (low >= 1 && low <= high && (dev_attr = ippFindAttribute(device->attrs, temp, IPP_TAG_ZERO)) != NULL)
      {
        size_t	i,			/* Looping var */
        	count = ippGetCount(attr),
        				/* New number of values */
        	dev_count = ippGetCount(dev_attr);
        				/* Current number of values */

	if (ippGetValueTag(attr) != ippGetValueTag(dev_attr) && ippGetValueTag(attr) != IPP_TAG_DELETEATTR)
	{
	  serverRespondUnsupported(client, attr);
	  continue;
	}
        else if (ippGetValueTag(attr) != IPP_TAG_DELETEATTR)
        {
          if (low < dev_count && count < (high - low + 1))
	  {
	   /*
	    * Deleting one or more values...
	    */

	    ippDeleteValues(device->attrs, &dev_attr, low - 1, high - low + 1 - count);
	  }
	  else if (high < dev_count && count > (high - low + 1))
	  {
	   /*
	    * Insert one or more values...
	    */

	    size_t offset = count - high + low - 1;
					  /* Number of values to insert */

	    switch (ippGetValueTag(dev_attr))
	    {
	      default :
		  break;

	      case IPP_TAG_BOOLEAN :
	          for (i = dev_count; i >= high; i --)
	            ippSetBoolean(device->attrs, &dev_attr, i + offset - 1, ippGetBoolean(dev_attr, i - 1));
		  break;

	      case IPP_TAG_INTEGER :
	      case IPP_TAG_ENUM :
	          for (i = dev_count; i >= high; i --)
	            ippSetInteger(device->attrs, &dev_attr, i + offset - 1, ippGetInteger(dev_attr, i - 1));
		  break;

	      case IPP_TAG_STRING :
	          for (i = dev_count; i >= high; i --)
	          {
	            size_t datalen;
	            void *data = ippGetOctetString(dev_attr, i - 1, &datalen);

	            ippSetOctetString(device->attrs, &dev_attr, i + offset - 1, data, datalen);
		  }
		  break;

	      case IPP_TAG_DATE :
	          for (i = dev_count; i >= high; i --)
	            ippSetDate(device->attrs, &dev_attr, i + offset - 1, ippGetDate(dev_attr, i - 1));
		  break;

	      case IPP_TAG_RESOLUTION :
	          for (i = dev_count; i >= high; i --)
	          {
	            int xres, yres;	/* Resolution */
	            ipp_res_t units;	/* Units */

	            xres = ippGetResolution(dev_attr, i - 1, &yres, &units);
	            ippSetResolution(device->attrs, &dev_attr, i + offset - 1, units, xres, yres);
		  }
		  break;

	      case IPP_TAG_RANGE :
	          for (i = dev_count; i >= high; i --)
	          {
	            int upper, lower = ippGetRange(dev_attr, i - 1, &upper);
					/* Range */

	            ippSetRange(device->attrs, &dev_attr, i + offset - 1, lower, upper);
		  }
		  break;

	      case IPP_TAG_BEGIN_COLLECTION :
	          for (i = dev_count; i >= high; i --)
	            ippSetCollection(device->attrs, &dev_attr, i + offset - 1, ippGetCollection(dev_attr, i - 1));
		  break;

	      case IPP_TAG_TEXTLANG :
	      case IPP_TAG_NAMELANG :
	      case IPP_TAG_TEXT :
	      case IPP_TAG_NAME :
	      case IPP_TAG_KEYWORD :
	      case IPP_TAG_URI :
	      case IPP_TAG_URISCHEME :
	      case IPP_TAG_CHARSET :
	      case IPP_TAG_LANGUAGE :
	      case IPP_TAG_MIMETYPE :
	          for (i = dev_count; i >= high; i --)
	            ippSetString(device->attrs, &dev_attr, i + offset - 1, ippGetString(dev_attr, i - 1, NULL));
		  break;
	    }
	  }
	}

	switch (ippGetValueTag(attr))
	{
          default : /* Don't allow updates for unknown values */
	      serverRespondUnsupported(client, attr);
              break;

	  case IPP_TAG_DELETEATTR :
	     /*
	      * Delete values from attribute...
	      */

	      if (low < count)
	      {
		if (high > count)
		  high = count;

		ippDeleteValues(device->attrs, &dev_attr, low - 1, high - low + 1);
	      }
	      break;

	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      for (i = high; i >= low; i --)
	        ippSetInteger(device->attrs, &dev_attr, i, ippGetInteger(attr, i - low));
	      break;

	  case IPP_TAG_BOOLEAN :
	      for (i = high; i >= low; i --)
	        ippSetBoolean(device->attrs, &dev_attr, i, ippGetBoolean(attr, i - low));
	      break;

          case IPP_TAG_STRING :
	      for (i = high; i >= low; i --)
	      {
	        size_t datalen;
	        void *data = ippGetOctetString(attr, i - low, &datalen);

	        ippSetOctetString(device->attrs, &dev_attr, i, data, datalen);
	      }
              break;

          case IPP_TAG_DATE :
	      for (i = high; i >= low; i --)
	        ippSetDate(device->attrs, &dev_attr, i, ippGetDate(attr, i - low));
              break;

          case IPP_TAG_RESOLUTION :
	      for (i = high; i >= low; i --)
	      {
	        int xres, yres;		/* Resolution */
	        ipp_res_t units;	/* Units */

                xres = ippGetResolution(attr, i - low, &yres, &units);
	        ippSetResolution(device->attrs, &dev_attr, i, units, xres, yres);
	      }
              break;

          case IPP_TAG_RANGE :
	      for (i = high; i >= low; i --)
	      {
	        int upper, lower = ippGetRange(attr, i - low, &upper);
					/* Range */

	        ippSetRange(device->attrs, &dev_attr, i, lower, upper);
	      }
              break;

          case IPP_TAG_BEGIN_COLLECTION :
	      for (i = high; i >= low; i --)
	        ippSetCollection(device->attrs, &dev_attr, i, ippGetCollection(attr, i - low));
              break;

          case IPP_TAG_TEXTLANG :
          case IPP_TAG_NAMELANG :
          case IPP_TAG_TEXT :
          case IPP_TAG_NAME :
          case IPP_TAG_KEYWORD :
          case IPP_TAG_URI :
          case IPP_TAG_URISCHEME :
          case IPP_TAG_CHARSET :
          case IPP_TAG_LANGUAGE :
          case IPP_TAG_MIMETYPE :
	      for (i = high; i >= low; i --)
	        ippSetString(device->attrs, &dev_attr, i, ippGetString(attr, i - low, NULL));
              break;
        }
      }
      else
        serverRespondUnsupported(client, attr);
    }
    else
    {
     /*
      * Regular representation - replace or delete current attribute,
      * if any...
      */

      if ((dev_attr = ippFindAttribute(device->attrs, attrname, IPP_TAG_ZERO)) != NULL)
        ippDeleteAttribute(device->attrs, dev_attr);

      if (ippGetValueTag(attr) != IPP_TAG_DELETEATTR)
        ippCopyAttribute(device->attrs, attr, 0);
    }
  }

  cupsRWUnlock(&device->rwlock);

  if (events)
  {
    cupsRWLockWrite(&client->printer->rwlock);
    if (events & SERVER_EVENT_PRINTER_CONFIG_CHANGED)
      serverUpdateDeviceAttributesNoLock(client->printer);
    if (events & SERVER_EVENT_PRINTER_STATE_CHANGED)
      serverUpdateDeviceStateNoLock(client->printer);
    cupsRWUnlock(&client->printer->rwlock);

    serverAddEventNoLock(client->printer, NULL, NULL, events, NULL);
  }
}


/*
 * 'ipp_validate_document()' - Validate document creation attributes.
 */

static void
ipp_validate_document(
    server_client_t *client)		/* I - Client */
{
  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if (valid_doc_attributes(client))
    serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'ipp_validate_job()' - Validate job creation attributes.
 */

static void
ipp_validate_job(server_client_t *client)	/* I - Client */
{
  if (Authentication && !client->username[0])
  {
   /*
    * Require authenticated username...
    */

    serverRespondHTTP(client, HTTP_STATUS_UNAUTHORIZED, NULL, NULL, 0);
    return;
  }

  if (Authentication && client->printer->pinfo.print_group != SERVER_GROUP_NONE && !serverAuthorizeUser(client, NULL, client->printer->pinfo.print_group, SERVER_SCOPE_DEFAULT))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_AUTHORIZED, "Not authorized to access this printer.");
    return;
  }

  if (valid_job_attributes(client))
    serverRespondIPP(client, IPP_STATUS_OK, NULL);
}


/*
 * 'serverProcessIPP()' - Process an IPP request.
 */

int					/* O - 1 on success, 0 on error */
serverProcessIPP(
    server_client_t *client)		/* I - Client */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
  int			major, minor;	/* Version number */
  const char		*name;		/* Name of attribute */


  serverLogAttributes(client, "Request:", client->request, 1);

 /*
  * First build an empty response message for this request...
  */

  client->operation_id = ippGetOperation(client->request);
  client->response     = ippNewResponse(client->request);

 /*
  * Then validate the request header and required attributes...
  */

  major = ippGetVersion(client->request, &minor);

  if (major < 1 || major > 2)
  {
   /*
    * Return an error, since we only support IPP 1.x and 2.x.
    */

    serverRespondIPP(client, IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED, "Bad request version number %d.%d.", major, minor);
    goto send_response;
  }
  else if (ippGetRequestId(client->request) <= 0)
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad request-id %d.", ippGetRequestId(client->request));
    goto send_response;
  }
  else if (!ippFirstAttribute(client->request))
  {
    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "No attributes in request.");
    goto send_response;
  }
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = ippFirstAttribute(client->request), group = ippGetGroupTag(attr); attr; attr = ippNextAttribute(client->request))
    {
      if (ippGetGroupTag(attr) < group && ippGetGroupTag(attr) != IPP_TAG_ZERO)
      {
       /*
	* Out of order; return an error...
	*/

	serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Attribute groups are out of order (%x < %x).", ippGetGroupTag(attr), group);
	goto send_response;
      }
      else
	group = ippGetGroupTag(attr);
    }

   /*
    * Then make sure that the first three attributes are:
    *
    *     attributes-charset
    *     attributes-natural-language
    *     printer-uri/job-uri
    */

    attr = ippFirstAttribute(client->request);
    name = ippGetName(attr);
    if (attr && name && !strcmp(name, "attributes-charset") && ippGetGroupTag(attr) == IPP_TAG_OPERATION && ippGetValueTag(attr) == IPP_TAG_CHARSET)
      charset = attr;
    else
      charset = NULL;

    attr = ippNextAttribute(client->request);
    name = ippGetName(attr);

    if (attr && name && !strcmp(name, "attributes-natural-language") && ippGetGroupTag(attr) == IPP_TAG_OPERATION && ippGetValueTag(attr) == IPP_TAG_LANGUAGE)
      language = attr;
    else
      language = NULL;

    attr = ippNextAttribute(client->request);
    name = ippGetName(attr);
    if (attr && name && (!strcmp(name, "system-uri") || !strcmp(name, "printer-uri") || !strcmp(name, "job-uri")) && ippGetGroupTag(attr) == IPP_TAG_OPERATION && ippGetValueTag(attr) == IPP_TAG_URI)
      uri = attr;
    else
      uri = NULL;

    if (!uri && RelaxedConformance)
    {
     /*
      * The target URI isn't where it is supposed to be.  See if it is
      * elsewhere in the request...
      */

      if ((attr = ippFindAttribute(client->request, "system-uri", IPP_TAG_URI)) != NULL && ippGetGroupTag(attr) == IPP_TAG_OPERATION)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "printer-uri", IPP_TAG_URI)) != NULL && ippGetGroupTag(attr) == IPP_TAG_OPERATION)
	uri = attr;
      else if ((attr = ippFindAttribute(client->request, "job-uri", IPP_TAG_URI)) != NULL && ippGetGroupTag(attr) == IPP_TAG_OPERATION)
	uri = attr;

      if (uri)
	serverLogClient(SERVER_LOGLEVEL_ERROR, client, "Target URI not the third attribute in the request (section 4.1.5 of RFC 8011).");
    }

    if (charset && strcasecmp(ippGetString(charset, 0, NULL), "us-ascii") && strcasecmp(ippGetString(charset, 0, NULL), "utf-8"))
    {
     /*
      * Bad character set...
      */

      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Unsupported character set \"%s\".", ippGetString(charset, 0, NULL));
      goto send_response;
    }
    else if (!charset || !language || !uri)
    {
     /*
      * Return an error, since attributes-charset,
      * attributes-natural-language, and printer-uri/job-uri are required
      * for all operations.
      */

      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing required attributes in request.");
      goto send_response;
    }
    else
    {
      char	scheme[32],		/* URI scheme */
		userpass[32],		/* Username/password in URI */
		host[256],		/* Host name in URI */
		resource[256],		/* Resource path in URI */
		*resptr;		/* Pointer into resource path */
      int	port;			/* Port number in URI */

      name            = ippGetName(uri);
      client->printer = NULL;

      if (httpSeparateURI(HTTP_URI_CODING_ALL, ippGetString(uri, 0, NULL), scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Bad \"%s\" value '%s'.", name, ippGetString(uri, 0, NULL));
	goto send_response;
      }
      else if (!strcmp(name, "job-uri"))
      {
       /*
	* Validate job-uri...
	*/

	if (strncmp(resource, "/ipp/print/", 11) && strncmp(resource, "/ipp/faxout/",12))
	{

	  serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "\"%s\" '%s' not found.", name, ippGetString(uri, 0, NULL));
	  goto send_response;
	}
	else
	{
	 /*
	  * Strip job-id from resource...
	  */
	  if ( !strncmp(resource,"/ipp/faxout/",12) )
	      resource[11] = '\0';
	  else if ((resptr = strchr(resource + 11, '/')) != NULL)
	    *resptr = '\0';
	  else
	    resource[10] = '\0';
	  if ((client->printer = serverFindPrinter(resource)) == NULL)
	  {
	    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "\"%s\" '%s' not found.", name, ippGetString(uri, 0, NULL));
	    goto send_response;
	  }
	}
      }
      else if ((client->printer = serverFindPrinter(resource)) == NULL)
      {
	if (strcmp(resource, "/ipp/system"))
	{
	  serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "\"%s\" '%s' not found.", name, ippGetString(uri, 0, NULL));
	  goto send_response;
	}
      }

      if (client->printer && client->printer->is_shutdown && ippGetOperation(client->request) != IPP_OP_STARTUP_PRINTER)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_SERVICE_UNAVAILABLE, "\"%s\" is shutdown.", client->printer->name);
	goto send_response;
      }
      else if (client->printer)
      {
       /*
	* Try processing the Printer operation...
	*/

	switch ((int)ippGetOperation(client->request))
	{
	  case IPP_OP_PRINT_JOB :
	      ipp_print_job(client);
	      break;

	  case IPP_OP_PRINT_URI :
	      ipp_print_uri(client);
	      break;

	  case IPP_OP_VALIDATE_JOB :
	      ipp_validate_job(client);
	      break;

	  case IPP_OP_CREATE_JOB :
	      ipp_create_job(client);
	      break;

	  case IPP_OP_SEND_DOCUMENT :
	      ipp_send_document(client);
	      break;

	  case IPP_OP_SEND_URI :
	      ipp_send_uri(client);
	      break;

	  case IPP_OP_CANCEL_JOB :
	      ipp_cancel_job(client);
	      break;

	  case IPP_OP_CANCEL_CURRENT_JOB :
	      ipp_cancel_current_job(client);
	      break;

	  case IPP_OP_CANCEL_JOBS :
	      ipp_cancel_jobs(client);
	      break;

	  case IPP_OP_CANCEL_MY_JOBS :
	      ipp_cancel_jobs(client);
	      break;

	  case IPP_OP_GET_JOB_ATTRIBUTES :
	      ipp_get_job_attributes(client);
	      break;

	  case IPP_OP_SET_JOB_ATTRIBUTES :
	      ipp_set_job_attributes(client);
	      break;

	  case IPP_OP_GET_JOBS :
	      ipp_get_jobs(client);
	      break;

	  case IPP_OP_GET_PRINTER_ATTRIBUTES :
	      ipp_get_printer_attributes(client);
	      break;

	  case IPP_OP_GET_PRINTER_SUPPORTED_VALUES :
	      ipp_get_printer_supported_values(client);
	      break;

	  case IPP_OP_SET_PRINTER_ATTRIBUTES :
	      ipp_set_printer_attributes(client);
	      break;

	  case IPP_OP_CLOSE_JOB :
	      ipp_close_job(client);
	      break;

	  case IPP_OP_HOLD_JOB :
	      ipp_hold_job(client);
	      break;

	  case IPP_OP_HOLD_NEW_JOBS :
	      ipp_hold_new_jobs(client);
	      break;

	  case IPP_OP_RELEASE_JOB :
	      ipp_release_job(client);
	      break;

	  case IPP_OP_RELEASE_HELD_NEW_JOBS :
	      ipp_release_held_new_jobs(client);
	      break;

	  case IPP_OP_IDENTIFY_PRINTER :
	      ipp_identify_printer(client);
	      break;

	  case IPP_OP_CANCEL_SUBSCRIPTION :
	      ipp_cancel_subscription(client);
	      break;

	  case IPP_OP_CREATE_JOB_SUBSCRIPTIONS :
	  case IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS :
	      ipp_create_xxx_subscriptions(client);
	      break;

	  case IPP_OP_GET_NOTIFICATIONS :
	      ipp_get_notifications(client);
	      break;

	  case IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES :
	      ipp_get_subscription_attributes(client);
	      break;

	  case IPP_OP_GET_SUBSCRIPTIONS :
	      ipp_get_subscriptions(client);
	      break;

	  case IPP_OP_RENEW_SUBSCRIPTION :
	      ipp_renew_subscription(client);
	      break;

	  case IPP_OP_CANCEL_DOCUMENT :
	      ipp_cancel_document(client);
	      break;

	  case IPP_OP_GET_DOCUMENT_ATTRIBUTES :
	      ipp_get_document_attributes(client);
	      break;

	  case IPP_OP_GET_DOCUMENTS :
	      ipp_get_documents(client);
	      break;

	  case IPP_OP_SET_DOCUMENT_ATTRIBUTES :
	      ipp_set_document_attributes(client);
	      break;

	  case IPP_OP_VALIDATE_DOCUMENT :
	      ipp_validate_document(client);
	      break;

	  case IPP_OP_ACKNOWLEDGE_DOCUMENT :
	      ipp_acknowledge_document(client);
	      break;

	  case IPP_OP_ACKNOWLEDGE_IDENTIFY_PRINTER :
	      ipp_acknowledge_identify_printer(client);
	      break;

	  case IPP_OP_ACKNOWLEDGE_JOB :
	      ipp_acknowledge_job(client);
	      break;

	  case IPP_OP_FETCH_DOCUMENT :
	      ipp_fetch_document(client);
	      break;

	  case IPP_OP_FETCH_JOB :
	      ipp_fetch_job(client);
	      break;

	  case IPP_OP_GET_OUTPUT_DEVICE_ATTRIBUTES :
	      ipp_get_output_device_attributes(client);
	      break;

	  case IPP_OP_UPDATE_ACTIVE_JOBS :
	      ipp_update_active_jobs(client);
	      break;

	  case IPP_OP_UPDATE_DOCUMENT_STATUS :
	      ipp_update_document_status(client);
	      break;

	  case IPP_OP_UPDATE_JOB_STATUS :
	      ipp_update_job_status(client);
	      break;

	  case IPP_OP_UPDATE_OUTPUT_DEVICE_ATTRIBUTES :
	      ipp_update_output_device_attributes(client);
	      break;

	  case IPP_OP_DEREGISTER_OUTPUT_DEVICE :
	      ipp_deregister_output_device(client);
	      break;

	  case IPP_OP_SHUTDOWN_PRINTER :
	      ipp_shutdown_printer(client);
	      break;

	  case IPP_OP_STARTUP_PRINTER :
	      ipp_startup_printer(client);
	      break;

	  case IPP_OP_RESTART_PRINTER :
	      ipp_restart_printer(client);
	      break;

	  case IPP_OP_DISABLE_PRINTER :
	      ipp_disable_printer(client);
	      break;

	  case IPP_OP_ENABLE_PRINTER :
	      ipp_enable_printer(client);
	      break;

	  case IPP_OP_PAUSE_PRINTER :
	  case IPP_OP_PAUSE_PRINTER_AFTER_CURRENT_JOB :
	      ipp_pause_printer(client);
	      break;

	  case IPP_OP_RESUME_PRINTER :
	      ipp_resume_printer(client);
	      break;

	  case IPP_OP_ALLOCATE_PRINTER_RESOURCES :
	      ipp_allocate_printer_resources(client);
	      break;

	  case IPP_OP_DEALLOCATE_PRINTER_RESOURCES :
	      ipp_deallocate_printer_resources(client);
	      break;

	  default :
	      serverRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
	      break;
	}
      }
      else if (!strcmp(resource, "/ipp/system"))
      {
       /*
	* Try processing the System operation...
	*/

	if ((attr = ippFindAttribute(client->request, "printer-id", IPP_TAG_INTEGER)) != NULL)
	{
	  int			printer_id = ippGetInteger(attr, 0);
					/* printer-id value */
	  server_printer_t	*printer;
					/* Current printer */


	  if (ippGetCount(attr) != 1 || ippGetGroupTag(attr) != IPP_TAG_OPERATION || printer_id <= 0)
	  {
	    serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad printer-id attribute.");
	    serverRespondUnsupported(client, attr);
	    goto send_response;
	  }

	  cupsRWLockRead(&PrintersRWLock);
	  for (printer = (server_printer_t *)cupsArrayGetFirst(Printers); printer; printer = (server_printer_t *)cupsArrayGetNext(Printers))
	  {
	    if (printer->id == printer_id)
	    {
	      client->printer = printer;
	      break;
	    }
	  }
	  cupsRWUnlock(&PrintersRWLock);

	  if (!client->printer)
	  {
	    serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Unknown printer-id.");
	    serverRespondUnsupported(client, attr);
	    goto send_response;
	  }
	}

	if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
	{
	  switch ((int)ippGetOperation(client->request))
	  {
	    case IPP_OP_GET_PRINTER_ATTRIBUTES :
		if (DefaultPrinter)
		{
		  client->printer = DefaultPrinter;
		  ipp_get_printer_attributes(client);
		}
		else
		{
		  serverRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "No default printer.");
		}
		break;

	    case IPP_OP_CANCEL_RESOURCE :
		ipp_cancel_resource(client);
		break;

	    case IPP_OP_CANCEL_SUBSCRIPTION :
		ipp_cancel_subscription(client);
		break;

	    case IPP_OP_CREATE_RESOURCE :
		ipp_create_resource(client);
		break;

	    case IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS :
		ipp_create_xxx_subscriptions(client);
		break;

	    case IPP_OP_GET_NOTIFICATIONS :
		ipp_get_notifications(client);
		break;

	    case IPP_OP_GET_RESOURCE_ATTRIBUTES :
		ipp_get_resource_attributes(client);
		break;

	    case IPP_OP_GET_RESOURCES :
		ipp_get_resources(client);
		break;

	    case IPP_OP_GET_SUBSCRIPTION_ATTRIBUTES :
		ipp_get_subscription_attributes(client);
		break;

	    case IPP_OP_GET_SUBSCRIPTIONS :
		ipp_get_subscriptions(client);
		break;

	    case IPP_OP_INSTALL_RESOURCE :
		ipp_install_resource(client);
		break;

	    case IPP_OP_RENEW_SUBSCRIPTION :
		ipp_renew_subscription(client);
		break;

	    case IPP_OP_SEND_RESOURCE_DATA :
		ipp_send_resource_data(client);
		break;

	    case IPP_OP_SET_RESOURCE_ATTRIBUTES :
		ipp_set_resource_attributes(client);
		break;

	    case IPP_OP_GET_SYSTEM_ATTRIBUTES :
		ipp_get_system_attributes(client);
		break;

	    case IPP_OP_GET_SYSTEM_SUPPORTED_VALUES :
		ipp_get_system_supported_values(client);
		break;

	    case IPP_OP_SET_SYSTEM_ATTRIBUTES :
		ipp_set_system_attributes(client);
		break;

	    case IPP_OP_CREATE_PRINTER :
		ipp_create_printer(client);
		break;

	    case IPP_OP_GET_PRINTERS :
		ipp_get_printers(client);
		break;

	    case IPP_OP_DELETE_PRINTER :
		if (client->printer)
		  ipp_delete_printer(client);
		else
		  serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing printer-id.");
		break;

	    case IPP_OP_DISABLE_ALL_PRINTERS :
		ipp_disable_all_printers(client);
		break;

	    case IPP_OP_ENABLE_ALL_PRINTERS :
		ipp_enable_all_printers(client);
		break;

	    case IPP_OP_PAUSE_ALL_PRINTERS :
	    case IPP_OP_PAUSE_ALL_PRINTERS_AFTER_CURRENT_JOB :
		ipp_pause_all_printers(client);
		break;

	    case IPP_OP_REGISTER_OUTPUT_DEVICE :
		ipp_register_output_device(client);
		break;

	    case IPP_OP_RESUME_ALL_PRINTERS :
		ipp_resume_all_printers(client);
		break;

	    case IPP_OP_SHUTDOWN_ALL_PRINTERS :
		ipp_shutdown_all_printers(client);
		break;

	    case IPP_OP_SHUTDOWN_ONE_PRINTER :
	        if (client->printer)
		  ipp_shutdown_printer(client);
		else
		  serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing printer-id.");
		break;

	    case IPP_OP_RESTART_SYSTEM :
		ipp_restart_system(client);
		break;

	    case IPP_OP_STARTUP_ALL_PRINTERS :
		ipp_startup_all_printers(client);
		break;

	    case IPP_OP_STARTUP_ONE_PRINTER :
	        if (client->printer)
		  ipp_startup_printer(client);
		else
		  serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing printer-id.");
		break;

	    default :
		serverRespondIPP(client, IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED, "Operation not supported.");
		break;
	  }
	}
      }
    }
  }

 /*
  * Send the HTTP header and return...
  */

  send_response:

  if (httpGetState(client->http) != HTTP_STATE_WAITING)
  {
    if (httpGetState(client->http) != HTTP_STATE_POST_SEND)
      httpFlush(client->http);		/* Flush trailing (junk) data */

    serverLogAttributes(client, "Response:", client->response, 2);

    return (serverRespondHTTP(client, HTTP_STATUS_OK, NULL, "application/ipp", client->fetch_file >= 0 ? 0 : ippLength(client->response)));
  }
  else
    return (1);
}


/*
 * 'serverRespondIPP()' - Send an IPP response.
 */

void
serverRespondIPP(
    server_client_t *client,		/* I - Client */
    ipp_status_t    status,		/* I - status-code */
    const char      *message,		/* I - printf-style status-message */
    ...)				/* I - Additional args as needed */
{
  const char	*formatted = NULL;	/* Formatted message */


  ippSetStatusCode(client->response, status);

  if (message)
  {
    va_list		ap;		/* Pointer to additional args */
    ipp_attribute_t	*attr;		/* New status-message attribute */

    va_start(ap, message);
    if ((attr = ippFindAttribute(client->response, "status-message",
				 IPP_TAG_TEXT)) != NULL)
      ippSetStringfv(client->response, &attr, 0, message, ap);
    else
      attr = ippAddStringfv(client->response, IPP_TAG_OPERATION, IPP_TAG_TEXT,
			    "status-message", NULL, message, ap);
    va_end(ap);

    formatted = ippGetString(attr, 0, NULL);
  }

  if (formatted)
    serverLogClient(SERVER_LOGLEVEL_INFO, client, "%s %s (%s)", ippOpString(client->operation_id), ippErrorString(status), formatted);
  else
    serverLogClient(SERVER_LOGLEVEL_INFO, client, "%s %s", ippOpString(client->operation_id), ippErrorString(status));
}


/*
 * 'serverRespondUnsupported()' - Respond with an unsupported attribute.
 */

void
serverRespondUnsupported(
    server_client_t *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Atribute */
{
  ipp_attribute_t	*temp;		/* Copy of attribute */


  if (ippGetStatusCode(client->response) == IPP_STATUS_OK)
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Unsupported %s %s%s value.", ippGetName(attr), ippGetCount(attr) > 1 ? "1setOf " : "", ippTagString(ippGetValueTag(attr)));

  temp = ippCopyAttribute(client->response, attr, 0);
  ippSetGroupTag(client->response, &temp, IPP_TAG_UNSUPPORTED_GROUP);
}


/*
 * 'respond_unsettable()' - Respond with an unsettable attribute.
 */

static void
respond_unsettable(
    server_client_t *client,		/* I - Client */
    ipp_attribute_t *attr)		/* I - Unsettable attribute */
{
  const char	*name = ippGetName(attr);
					/* Attribute name */


  if (ippGetStatusCode(client->response) != IPP_STATUS_OK)
    serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_NOT_SETTABLE, "Unsettable %s attribute.", name);

  ippAddOutOfBand(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_NOTSETTABLE, name);
}


/*
 * 'valid_doc_attributes()' - Determine whether the document attributes are
 *                            valid.
 *
 * When one or more document attributes are invalid, this function adds a
 * suitable response and attributes to the unsupported group.
 */

static bool				/* O - `true` if valid, `false` if not */
valid_doc_attributes(
    server_client_t *client)		/* I - Client */
{
  bool			valid = true;	/* Valid attributes? */
  ipp_op_t		op = ippGetOperation(client->request);
					/* IPP operation */
  const char		*op_name = ippOpString(op);
					/* IPP operation name */
  ipp_attribute_t	*attr,		/* Current attribute */
			*supported;	/* xxx-supported attribute */
  const char		*compression = NULL,
					/* compression value */
			*format = NULL;	/* document-format value */


 /*
  * Check operation attributes...
  */

  if ((attr = ippFindAttribute(client->request, "compression", IPP_TAG_ZERO)) != NULL)
  {
   /*
    * If compression is specified, only accept a supported value in a Print-Job
    * or Send-Document request...
    */

    compression = ippGetString(attr, 0, NULL);
    supported   = ippFindAttribute(client->printer->pinfo.attrs,
                                   "compression-supported", IPP_TAG_KEYWORD);

    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_KEYWORD ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION ||
        (op != IPP_OP_PRINT_JOB && op != IPP_OP_SEND_DOCUMENT &&
         op != IPP_OP_VALIDATE_JOB) ||
        !ippContainsString(supported, compression))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
    else
    {
      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "%s compression='%s'", op_name, compression);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "compression-supplied", NULL, compression);

      if (strcmp(compression, "none"))
        httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, compression);
    }
  }

 /*
  * Is it a format we support?
  */

  if ((attr = ippFindAttribute(client->request, "document-format", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_MIMETYPE ||
        ippGetGroupTag(attr) != IPP_TAG_OPERATION)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
    else
    {
      format = ippGetString(attr, 0, NULL);

      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "%s document-format='%s'", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-supplied", NULL, format);
fprintf(stderr,"HERE  %s\n",format);
    }
  }
  else
  {
    format = ippGetString(ippFindAttribute(client->printer->pinfo.attrs, "document-format-default", IPP_TAG_MIMETYPE), 0, NULL);
    if (!format)
      format = "application/octet-stream"; /* Should never happen */

    attr = ippAddString(client->request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
  }

  if ((!format || !strcmp(format, "application/octet-stream")) && (ippGetOperation(client->request) == IPP_OP_PRINT_JOB || ippGetOperation(client->request) == IPP_OP_SEND_DOCUMENT))
  {
   /*
    * Auto-type the file using the first 8 bytes of the file...
    */

    unsigned char	header[8];	/* First 8 bytes of file */

    memset(header, 0, sizeof(header));
    httpPeek(client->http, (char *)header, sizeof(header));

    if ((format = detect_format(header)) != NULL)
    {
      serverLogClient(SERVER_LOGLEVEL_DEBUG, client, "%s Auto-typed document-format='%s'", op_name, format);

      ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format-detected", NULL, format);
    }
  }

  if ((op == IPP_OP_PRINT_JOB || op == IPP_OP_SEND_DOCUMENT) && (supported = ippFindAttribute(client->printer->pinfo.attrs, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL && !ippContainsString(supported, format) && attr && ippGetGroupTag(attr) == IPP_TAG_OPERATION)
  {
    serverRespondUnsupported(client, attr);
    valid = false;
  }

  return (valid);
}


/*
 * 'valid_filename()' - Make sure the filename in a file: URI is allowed.
 */

static bool				/* O - `true` if valid, `false` if not */
valid_filename(const char *filename)	/* I - Filename to validate */
{
  size_t	i,			/* Looping var */
		count = cupsArrayGetCount(FileDirectories);
					/* Number of directories */
  char		*dir;			/* Current directory */
  size_t	filelen = strlen(filename),
					/* Length of filename */
		dirlen;			/* Length of directory */


 /*
  * Do not allow filenames containing "something/../something" or
  * "something/./something"...
  */

  if (strstr(filename, "/../") || strstr(filename, "/./"))
    return (false);

 /*
  * Check for prefix matches on any of the directories...
  */

  for (i = 0; i < count; i ++)
  {
    dir    = (char *)cupsArrayGetElement(FileDirectories, i);
    dirlen = strlen(dir);

    if (filelen >= dirlen && strncmp(filename, dir, dirlen) && (filename[dirlen] == '/' || !filename[dirlen]))
      return (true);
  }

  return (false);
}


/*
 * 'valid_job_attributes()' - Determine whether the job attributes are valid.
 *
 * When one or more job attributes are invalid, this function adds a suitable
 * response and attributes to the unsupported group.
 */

static bool				/* O - `true` if valid, `false` if not */
valid_job_attributes(
    server_client_t *client)		/* I - Client */
{
  size_t		i,		/* Looping var */
			count;		/* Number of values */
  bool			valid = true;	/* Valid attributes? */
  ipp_attribute_t	*attr,		/* Request attribute */
			*resource_ids,	/* resources-ids attribute */
			*supported;	/* Supported attribute */
  int			resource_id;	/* Resource ID value */
  server_resource_t	*resource;	/* Resource */
  ipp_op_t		op = ippGetOperation(client->request);
					/* Current operation */


 /*
  * Check attributes in request...
  */

  cupsRWLockRead(&client->printer->rwlock);

  supported = ippFindAttribute(client->printer->pinfo.attrs, "job-creation-attributes-suppored", IPP_TAG_KEYWORD);

  if ((resource_ids = ippFindAttribute(client->request, "resource-ids", IPP_TAG_INTEGER)) != NULL)
  {
    if (ippGetGroupTag(resource_ids) != IPP_TAG_OPERATION)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The 'resource-ids' attribute is not in the operation group.");
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&client->printer->rwlock);
      return (false);
    }
    else if ((count = ippGetCount(resource_ids)) > SERVER_RESOURCES_MAX)
    {
      serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Too many resources (%u) specified.", (unsigned)count);
      serverRespondUnsupported(client, resource_ids);
      cupsRWUnlock(&client->printer->rwlock);
      return (false);
    }

    for (i = 0; i < count; i ++)
    {
      resource_id = ippGetInteger(resource_ids, i);

      if ((resource = serverFindResourceById(resource_id)) == NULL)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d not found.", resource_id);
	serverRespondUnsupported(client, resource_ids);
	cupsRWUnlock(&client->printer->rwlock);
	return (false);
      }
      else if (resource->state != IPP_RSTATE_INSTALLED)
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is not installed (%s).", resource_id, ippEnumString("resource-state", (int)resource->state));
	serverRespondUnsupported(client, resource_ids);
	cupsRWUnlock(&client->printer->rwlock);
	return (false);
      }
      else if (!strcmp(resource->type, "template-job"))
      {
        if (!apply_template_attributes(client->request, IPP_TAG_JOB, resource, supported, sizeof(job_values) / sizeof(job_values[0]), job_values))
        {
          serverRespondIPP(client, IPP_STATUS_ERROR_INTERNAL, "Unable to apply template-job resource #%d: %s", resource_id, cupsLastErrorString());
	  cupsRWUnlock(&client->printer->rwlock);
	  return (false);
        }
      }
      else
      {
	serverRespondIPP(client, IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES, "Resource #%d is the wrong type (%s).", resource_id, resource->type);
	serverRespondUnsupported(client, resource_ids);
	cupsRWUnlock(&client->printer->rwlock);
	return (false);
      }
    }
  }

  if (!valid_values(client, IPP_TAG_JOB, supported, sizeof(job_values) / sizeof(job_values[0]), job_values))
  {
    cupsRWUnlock(&client->printer->rwlock);
    return (false);
  }

  cupsRWUnlock(&client->printer->rwlock);

 /*
  * Check operation attributes...
  */

  valid = valid_doc_attributes(client);

 /*
  * Check the various job template attributes...
  */

  if ((attr = ippFindAttribute(client->request, "copies", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 999)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "ipp-attribute-fidelity", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_BOOLEAN)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-hold-until", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->pinfo.attrs, "job-hold-until-supported", IPP_TAG_ZERO);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-impressions", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetInteger(attr, 0) < 0)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-name", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 ||
        (ippGetValueTag(attr) != IPP_TAG_NAME &&
	 ippGetValueTag(attr) != IPP_TAG_NAMELANG))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }

    ippSetGroupTag(client->request, &attr, IPP_TAG_JOB);
  }
  else if (op == IPP_OP_CREATE_JOB || op == IPP_OP_PRINT_JOB || op == IPP_OP_PRINT_URI)
    ippAddString(client->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, "Untitled");

  if ((attr = ippFindAttribute(client->request, "job-priority", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetCount(attr) != 1 || ippGetValueTag(attr) != IPP_TAG_INTEGER ||
        ippGetInteger(attr, 0) < 1 || ippGetInteger(attr, 0) > 100)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "job-sheets", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->pinfo.attrs, "job-sheets-supported", IPP_TAG_ZERO);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media", IPP_TAG_ZERO)) != NULL)
  {
    if ((supported = ippFindAttribute(client->printer->dev_attrs, "media-supported", IPP_TAG_KEYWORD)) == NULL)
      supported = ippFindAttribute(client->printer->pinfo.attrs, "media-supported", IPP_TAG_KEYWORD);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "media-col", IPP_TAG_ZERO)) != NULL)
  {
    ipp_t		*col,		/* media-col collection */
			*size;		/* media-size collection */
    ipp_attribute_t	*member,	/* Member attribute */
			*x_dim,		/* x-dimension */
			*y_dim;		/* y-dimension */
    int			x_value,	/* y-dimension value */
			y_value;	/* x-dimension value */

    col = ippGetCollection(attr, 0);

    if ((member = ippFindAttribute(col, "media-size-name", IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetCount(member) != 1 ||
	  (ippGetValueTag(member) != IPP_TAG_NAME &&
	   ippGetValueTag(member) != IPP_TAG_NAMELANG &&
	   ippGetValueTag(member) != IPP_TAG_KEYWORD))
      {
	serverRespondUnsupported(client, attr);
	valid = false;
      }
      else
      {
        if ((supported = ippFindAttribute(client->printer->dev_attrs, "media-supported", IPP_TAG_KEYWORD)) == NULL)
	  supported = ippFindAttribute(client->printer->pinfo.attrs, "media-supported", IPP_TAG_KEYWORD);

	if (!ippContainsString(supported, ippGetString(member, 0, NULL)))
	{
	  serverRespondUnsupported(client, attr);
	  valid = false;
	}
      }
    }
    else if ((member = ippFindAttribute(col, "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      if (ippGetCount(member) != 1)
      {
	serverRespondUnsupported(client, attr);
	valid = false;
      }
      else
      {
	size = ippGetCollection(member, 0);

	if ((supported = ippFindAttribute(client->printer->dev_attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION)) == NULL)
	  supported = ippFindAttribute(client->printer->pinfo.attrs, "media-size-supported", IPP_TAG_BEGIN_COLLECTION);

	if ((x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(x_dim) != 1 ||
	    (y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_INTEGER)) == NULL || ippGetCount(y_dim) != 1)
	{
	  serverRespondUnsupported(client, attr);
	  valid = false;
	}
	else if (supported)
	{
	  x_value   = ippGetInteger(x_dim, 0);
	  y_value   = ippGetInteger(y_dim, 0);
	  count     = ippGetCount(supported);

	  for (i = 0; i < count ; i ++)
	  {
	    size  = ippGetCollection(supported, i);
	    x_dim = ippFindAttribute(size, "x-dimension", IPP_TAG_ZERO);
	    y_dim = ippFindAttribute(size, "y-dimension", IPP_TAG_ZERO);

	    if (ippContainsInteger(x_dim, x_value) && ippContainsInteger(y_dim, y_value))
	      break;
	  }

	  if (i >= count)
	  {
	    serverRespondUnsupported(client, attr);
	    valid = false;
	  }
	}
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "multiple-document-handling", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->pinfo.attrs, "multiple-document-handling-supported", IPP_TAG_KEYWORD);

    if (!ippContainsString(supported, ippGetString(attr, 0, NULL)))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "orientation-requested", IPP_TAG_ZERO)) != NULL)
  {
    supported = ippFindAttribute(client->printer->pinfo.attrs, "orientation-requested-supported", IPP_TAG_ENUM);

    if (!ippContainsInteger(supported, ippGetInteger(attr, 0)))
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "print-quality", IPP_TAG_ZERO)) != NULL)
  {
    if (ippGetInteger(attr, 0) < IPP_QUALITY_DRAFT || ippGetInteger(attr, 0) > IPP_QUALITY_HIGH)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
  }

  if ((attr = ippFindAttribute(client->request, "printer-resolution", IPP_TAG_ZERO)) != NULL)
  {
    if ((supported = ippFindAttribute(client->printer->dev_attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION)) == NULL)
      supported = ippFindAttribute(client->printer->pinfo.attrs, "printer-resolution-supported", IPP_TAG_RESOLUTION);

    if (!supported)
    {
      serverRespondUnsupported(client, attr);
      valid = false;
    }
    else
    {
      int	xdpi,			/* Horizontal resolution for job template attribute */
		ydpi,			/* Vertical resolution for job template attribute */
		sydpi;			/* Vertical resolution for supported value */
      ipp_res_t	units,			/* Units for job template attribute */
		sunits;			/* Units for supported value */

      xdpi  = ippGetResolution(attr, 0, &ydpi, &units);
      count = ippGetCount(supported);

      for (i = 0; i < count; i ++)
      {
        if (xdpi == ippGetResolution(supported, i, &sydpi, &sunits) && ydpi == sydpi && units == sunits)
          break;
      }

      if (i >= count)
      {
	serverRespondUnsupported(client, attr);
	valid = false;
      }
    }
  }

  if ((attr = ippFindAttribute(client->request, "sides", IPP_TAG_ZERO)) != NULL)
  {
    const char *sides = ippGetString(attr, 0, NULL);
					/* "sides" value... */

    if ((supported = ippFindAttribute(client->printer->dev_attrs, "sides-supported", IPP_TAG_KEYWORD)) == NULL)
      supported = ippFindAttribute(client->printer->pinfo.attrs, "sides-supported", IPP_TAG_KEYWORD);

    if (!ippContainsString(supported, sides) && strcmp(sides, "one-sided"))
    {
      if (!ippContainsString(supported, sides))
      {
	serverRespondUnsupported(client, attr);
	valid = false;
      }
    }
  }

  return (valid);
}


/*
 * 'valid_values()' - Check whether attributes in the specified group are valid.
 */

static bool				/* O - `true` if valid, `false` if not */
valid_values(
    server_client_t *client,		/* I - Client connection */
    ipp_tag_t       group_tag,		/* I - Group to check */
    ipp_attribute_t *supported,		/* I - List of supported attributes */
    size_t          num_values,		/* I - Number of values to check */
    server_value_t  *values)		/* I - Values to check */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_tag_t		value_tag;	/* Value tag for attribute */
  ipp_op_t		op = ippGetOperation(client->request);
					/* Operation code */
  int			create_op,	/* Is this a create operation? */
			set_op;		/* Is this a set operation? */


  create_op = (op == IPP_OP_CREATE_JOB || op == IPP_OP_CREATE_PRINTER || op == IPP_OP_CREATE_RESOURCE || op == IPP_OP_PRINT_JOB || op == IPP_OP_PRINT_URI || op == IPP_OP_VALIDATE_JOB || op == IPP_OP_VALIDATE_DOCUMENT);
  set_op    = (op == IPP_OP_SET_DOCUMENT_ATTRIBUTES || op == IPP_OP_SET_JOB_ATTRIBUTES || op == IPP_OP_SET_PRINTER_ATTRIBUTES || op == IPP_OP_SET_RESOURCE_ATTRIBUTES || op == IPP_OP_SET_SYSTEM_ATTRIBUTES);

  if (supported)
  {
    for (attr = ippFirstAttribute(client->request); attr; attr = ippNextAttribute(client->request))
    {
      const char *name = ippGetName(attr);/* Attribute name */

      if (!name || ippGetGroupTag(attr) != group_tag)
        continue;

      if (!ippContainsString(supported, name))
      {
        if (set_op)
	  respond_unsettable(client, attr);
	else
	  serverRespondUnsupported(client, attr);
        return (false);
      }
    }
  }

  while (num_values > 0)
  {
    if ((attr = ippFindAttribute(client->request, values->name, IPP_TAG_ZERO)) != NULL)
    {
      if (ippGetGroupTag(attr) != group_tag && (!(values->flags & VALUE_CREATEOP) || !create_op || ippGetGroupTag(attr) != IPP_TAG_OPERATION))
      {
        serverRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "'%s' attribute in the wrong group.", values->name);
        serverRespondUnsupported(client, attr);
        return (false);
      }

      value_tag = ippGetValueTag(attr);

      if (value_tag != values->value_tag && value_tag != values->alt_tag && (value_tag != IPP_TAG_NAMELANG || values->value_tag != IPP_TAG_NAME) && (value_tag != IPP_TAG_TEXTLANG || values->value_tag != IPP_TAG_TEXT))
      {
        serverRespondUnsupported(client, attr);
        return (false);
      }

      if (ippGetCount(attr) > 1 && !(values->flags & VALUE_1SETOF))
      {
        serverRespondUnsupported(client, attr);
        return (false);
      }
    }

    values ++;
    num_values --;
  }

  return (true);
}


/*
 * 'wgs84_distance()' - Approximate the distance between two geo: values.
 */

#ifndef M_PI				/* Should never happen, but happens on Windows... */
#  define M_PI		3.14159265358979323846
#endif /* !M_PI */
#define M_PER_DEG	111120.0	/* Meters per degree of latitude */

static float				/* O - Distance in meters */
wgs84_distance(const char *a,		/* I - First geo: value */
               const char *b)		/* I - Second geo: value */
{
  char		*ptr;			/* Pointer into string */
  double	a_lat, a_lon, a_alt;	/* First latitude, longitude, altitude */
  double	b_lat, b_lon, b_alt;	/* First latitude, longitude, altitude */
  double	d_lat, d_lon, d_alt;	/* Difference in positions */


 /*
  * Decode the geo: values...
  */

  a_lat = strtod(a + 4, &ptr);
  if (*ptr != ',')
    return (999999.0);			/* Large value = error */
  a_lon = strtod(ptr + 1, &ptr);
  if (*ptr != ',')
    a_alt = 0.0;
  else
    a_alt = strtod(ptr + 1, NULL);

  b_lat = strtod(b + 4, &ptr);
  if (*ptr != ',')
    return (999999.0);			/* Large value = error */
  b_lon = strtod(ptr + 1, &ptr);
  if (*ptr != ',')
    b_alt = 0.0;
  else
    b_alt = strtod(ptr + 1, NULL);

 /*
  * Approximate the distance between the two points.
  *
  * Note: This calculation is not meant to be used for navigation or other
  * serious uses of WGS-84 coordinates.  Rather, we are simply calculating the
  * angular distance between the two coordinates on a sphere (vs. the WGS-84
  * ellipsoid) and then multiplying by an approximate number of meters between
  * each degree of latitude and longitude.  The error bars on this calculation
  * are reasonable for local comparisons (<1m error over 1 degree of latitude
  * change) and completely unreasonable for distant comparisons.  You have been
  * warned! :)
  */

  d_lat = M_PER_DEG * (a_lat - b_lat);
  d_lon = M_PER_DEG * cos((a_lat + b_lat) * M_PI / 360.0) * (a_lon - b_lon);
  d_alt = a_alt - b_alt;

  return ((float)sqrt(d_lat * d_lat + d_lon * d_lon + d_alt * d_alt));
}
