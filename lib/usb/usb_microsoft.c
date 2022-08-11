/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2022 Rachel Mant <git@dragonmux.network>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/microsoft.h>
#include "usb_private.h"

static uint16_t build_function_subset(const microsoft_os_descriptor_function_subset_header *const subset,
	uint8_t *const buf, uint16_t len)
{
	uint16_t count = MIN(len, subset->wLength);
	memcpy(buf, subset, count);
	uint16_t total_length = subset->wLength;

	((microsoft_os_descriptor_function_subset_header *)buf)->wTotalLength = total_length;
	return total_length;
}

static uint16_t build_config_subset(const microsoft_os_descriptor_config_subset_header *const subset,
	uint8_t *const buf, uint16_t len)
{
	uint16_t total = 0;
	uint16_t count = MIN(len, subset->wLength);
	memcpy(buf, subset, count);
	len -= count;
	total += count;
	uint16_t total_length = subset->wLength;

	for (size_t i = 0; i < subset->num_function_subset_headers; ++i) {
		const uint16_t subset_header_len = build_function_subset(&subset->function_subset_headers[i],
			buf + total, len);
		count = MIN(len, subset_header_len);
		len -= count;
		total += count;
		total_length += subset_header_len;
	}

	((microsoft_os_descriptor_config_subset_header *)buf)->wTotalLength = total_length;
	return total_length;
}

static uint16_t build_descriptor_set(const microsoft_os_descriptor_set_header *const set,
	uint8_t *const buf, uint16_t len)
{
	uint16_t count = MIN(len, set->wLength);
	memcpy(buf, set, count);
	len -= count;
	uint16_t total = count;
	uint16_t total_length = set->wLength;

	for (size_t i = 0; i < set->num_config_subset_headers; ++i) {
		const uint16_t subset_header_len = build_config_subset(&set->config_subset_headers[i], buf + total, len);
		count = MIN(len, subset_header_len);
		len -= count;
		total += count;
		total_length += subset_header_len;
	}

	((microsoft_os_descriptor_set_header *)buf)->wTotalLength = total_length;
	return total;
}

static enum usbd_request_return_codes microsoft_os_get_descriptor_set(usbd_device *const usbd_dev,
	struct usb_setup_data *const req, uint8_t **const buf, uint16_t *const len)
{
	if (req->wValue != 0)
		return USBD_REQ_NOTSUPP;

	const microsoft_os_descriptor_set_header *const sets = usbd_dev->microsoft_os_descriptor_sets;
	for (size_t i = 0; i < usbd_dev->num_microsoft_os_descriptor_sets; ++i) {
		if (sets[i].vendor_code == req->bRequest) {
			*buf = usbd_dev->ctrl_buf;
			*len = build_descriptor_set(&sets[i], *buf, *len);
			return USBD_REQ_HANDLED;
		}
	}
	return USBD_REQ_NOTSUPP;
}

static enum usbd_request_return_codes microsoft_os_control_request(usbd_device *const usbd_dev,
	struct usb_setup_data *const req, uint8_t **const buf, uint16_t *const len)
{
	switch (req->wIndex) {
	case MICROSOFT_GET_DESCRIPTOR_SET:
		return microsoft_os_get_descriptor_set(usbd_dev, req, buf, len);
	case MICROSOFT_SET_ALTERNATE_ENUM:
		return USBD_REQ_NOTSUPP;
	}
	return USBD_REQ_NEXT_CALLBACK;
}

void microsoft_os_register_descriptor_sets(usbd_device *const dev,
	const microsoft_os_descriptor_set_header *const sets, const uint8_t num_sets)
{
	dev->microsoft_os_req_callback = microsoft_os_control_request;
	dev->microsoft_os_descriptor_sets = sets;
	dev->num_microsoft_os_descriptor_sets = num_sets;
}
