#!/usr/bin/python3

import traceback
import sys
import gi

gi.require_version('FPrint', '2.0')
from gi.repository import FPrint, GLib

sys.excepthook = lambda *args: (traceback.print_exception(*args), sys.exit(1))

ctx = GLib.main_context_default()

c = FPrint.Context()
c.enumerate()
devices = c.get_devices()

d = next((dev for dev in devices if dev.get_driver() == "focaltech_moc"), None)

if d is None:
    print("Error: Focaltech driver not loaded for any discovered device.")
    sys.exit(1)

del devices

d.open_sync()
assert d.get_driver() == "focaltech_moc"
assert d.has_feature(FPrint.DeviceFeature.STORAGE_LIST)
assert d.has_feature(FPrint.DeviceFeature.STORAGE_DELETE)
#added storage clear
assert d.has_feature(FPrint.DeviceFeature.STORAGE_CLEAR)


print("Clearing storage (0xAC)...")
d.clear_storage_sync()

template = FPrint.Print.new(d)

def enroll_progress(*args):
    print('Enroll progress: ' + str(args))

print("Enrolling (Perform 12 taps now)...")
p = d.enroll_sync(template, None, enroll_progress, None)
print("Enroll done.")

print("Listing prints...")
stored = d.list_prints_sync()
assert len(stored) == 1
assert stored[0].equal(p)
print("List verified.")

print("Verifying...")
verify_res, verify_print = d.verify_sync(p)
assert verify_res == True
print("Verify done.")

print('Identifying...')
match, identify_print = d.identify_sync(stored, None, None, None)

if match:
    print('Identification SUCCESS')
    assert match.equal(identify_print)
else:
    print('Identification FAILURE')
    sys.exit(1)

print("Deleting...")
try:
    d.delete_print_sync(stored[0])
except GLib.Error as error:
    assert error.matches(FPrint.DeviceError.quark(),
                         FPrint.DeviceError.NOT_SUPPORTED)
else:
    raise AssertionError("delete_print_sync unexpectedly succeeded")
print("Delete not supported verified.")

print("Clearing storage (0xAC)...")
d.clear_storage_sync()

final_list = d.list_prints_sync()
assert len(final_list) == 0
print("Cleanup verified.")

d.close_sync()
