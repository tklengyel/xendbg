//
// Copyright (C) 2018-2019 NCC Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <sys/mman.h>

#include <Xen/HVMMonitor.hpp>

/* From xen/include/asm-x86/processor.h */
#define X86_TRAP_DEBUG  1
#define X86_TRAP_INT3   3

using xd::xen::Domain;
using xd::xen::HVMMonitor;

HVMMonitor::HVMMonitor(xen::XenDeviceModel &xendevicemodel,
    xen::XenEventChannel &xenevtchn, uvw::Loop &loop, DomainHVM &domain)
  : _xendevicemodel(xendevicemodel), _xenevtchn(xenevtchn), _domain(domain),
    _port(0), _ring_page(nullptr, unmap_ring_page),
    _poll(loop.resource<uvw::PollHandle>(xenevtchn.get_fd()))
{
}

HVMMonitor::~HVMMonitor() {
  if (_port != 0)
    _xenevtchn.unbind(_port);
}

void HVMMonitor::start() {
  auto [ring_page, evtchn_port] = _domain.enable_monitor(); // TODO

  _ring_page.reset(ring_page);
  _port = _xenevtchn.bind_interdomain(_domain, evtchn_port);

  SHARED_RING_INIT((vm_event_sring_t*)ring_page);
  BACK_RING_INIT(&_back_ring, (vm_event_sring_t*)ring_page, XC_PAGE_SIZE);

  _domain.monitor_singlestep(true);
  _domain.monitor_software_breakpoint(true);
  //_domain.monitor_debug_exceptions(true, true);
  //_domain.monitor_cpuid(true);
  //_domain.monitor_descriptor_access(true);
  //_domain.monitor_privileged_call(true);

  // TODO: this capture fails if moved
  const auto l_port = _port;
  _poll->data(shared_from_this());
  _poll->on<uvw::PollEvent>([l_port](const auto &event, auto &handle) {
      auto self = handle.template data<HVMMonitor>();
      const auto port = self->_xenevtchn.get_next_pending_channel();
      if (port == l_port)
        self->read_events();
      self->_xenevtchn.unmask_channel(port);
  });

  _poll->start(uvw::PollHandle::Event::READABLE);
}

void HVMMonitor::stop() {
  _domain.monitor_singlestep(false);
  _domain.monitor_software_breakpoint(false);
  //_domain.monitor_debug_exceptions(false, false);
  //_domain.monitor_cpuid(false);
  //_domain.monitor_descriptor_access(false);
  //_domain.monitor_privileged_call(false);
  _domain.disable_monitor();

  if (!_poll->closing())
    _poll->stop();
}

vm_event_request_t HVMMonitor::get_request() {
    vm_event_request_t req;
    RING_IDX req_cons;

    req_cons = _back_ring.req_cons;

    /* Copy request */
    memcpy(&req, RING_GET_REQUEST(&_back_ring, req_cons), sizeof(req));
    req_cons++;

    /* Update ring */
    _back_ring.req_cons = req_cons;
    _back_ring.sring->req_event = req_cons + 1;

    return req;
}

void HVMMonitor::put_response(vm_event_response_t rsp) {
    RING_IDX rsp_prod;

    rsp_prod = _back_ring.rsp_prod_pvt;

    /* Copy response */
    memcpy(RING_GET_RESPONSE(&_back_ring, rsp_prod), &rsp, sizeof(rsp));
    rsp_prod++;

    /* Update ring */
    _back_ring.rsp_prod_pvt = rsp_prod;
    RING_PUSH_RESPONSES(&_back_ring);
}

void HVMMonitor::read_events() {
  while (RING_HAS_UNCONSUMED_REQUESTS(&_back_ring)) {
    auto req = get_request();

    vm_event_response_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.version = VM_EVENT_INTERFACE_VERSION;
    rsp.vcpu_id = req.vcpu_id;
    rsp.flags = req.flags & VM_EVENT_FLAG_VCPU_PAUSED;

    rsp.reason = req.reason;

    if (req.version != VM_EVENT_INTERFACE_VERSION)
      continue; // TODO: error

    if (_on_event)
      _on_event(req);

    put_response(rsp);
  }

  _xenevtchn.notify(_port);
}

void HVMMonitor::unmap_ring_page(void *ring_page) {
  if (ring_page)
    munmap(ring_page, XC_PAGE_SIZE);
}
