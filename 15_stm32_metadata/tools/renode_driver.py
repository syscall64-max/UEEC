# -*- coding: utf-8 -*-
"""Executed by Renode's embedded IronPython, not by host Python 3.

The host runner prepends a JSON CONFIG object. No firmware states are generated
here: LED events come from the CPU-driven GPIO peripheral model.
"""
import json
import struct
import clr
clr.AddReference('System.Diagnostics.Process')
from System.Diagnostics import Process, ProcessStartInfo
from System.IO import File

def run():
    machine=monitor.Machine
    bus=machine.SystemBus
    cpuid=int(bus.ReadDoubleWord(0xe000ed00))
    flash_bytes=int(machine['sysbus.flash'].Size)
    ram_bytes=int(machine['sysbus.sram'].Size)
    clock=int(machine['sysbus.nvic'].Frequency)
    print('PROBE model={0} cpuid=0x{1:08x} Flash={2} RAM={3} clock={4}'.format(
          CONFIG['model'],cpuid,flash_bytes,ram_bytes,clock))
    profile=struct.pack('<4sHHIIIIIIQIBBH','STP1',CONFIG['model_id'],0,cpuid,
                        0x08000000,flash_bytes,0x20000000,ram_bytes,clock,
                        CONFIG['capabilities'],CONFIG['gpio_base'],ord(CONFIG['port']),CONFIG['pin'],0)
    with open(CONFIG['device_profile'],'wb') as output: output.write(profile)
    info=ProcessStartInfo()
    info.FileName=CONFIG['selector']
    info.UseShellExecute=False
    info.RedirectStandardOutput=True
    arguments=['--select','--device-profile',CONFIG['device_profile']]
    if CONFIG['list_nodes']: arguments+=['--list-nodes']
    else: arguments+=['--extract',CONFIG['code'],'--manifest',CONFIG['manifest']]
    arguments+=[CONFIG['source']]
    for argument in arguments: info.ArgumentList.Add(argument)
    process=Process.Start(info)
    output=process.StandardOutput.ReadToEnd()
    process.WaitForExit()
    print(output)
    if process.ExitCode!=0: raise Exception('C selector rejected the UEEC/device profile')
    if CONFIG['list_nodes'] or CONFIG['no_run']:
        print('UEEC_SELECTION_PASS')
        return
    selected=json.loads(File.ReadAllText(CONFIG['manifest']))
    image=File.ReadAllBytes(CONFIG['code'])
    if len(image)!=selected['code_bytes']: raise Exception('extracted CODE size mismatch')
    bus.WriteBytes(image,selected['flash_base'])
    readback=bus.ReadBytes(selected['flash_base'],len(image))
    if list(image)!=list(readback): raise Exception('virtual Flash readback mismatch')
    print('FLASH bytes={0} address=0x{1:08x} verified=yes metadata_written=no'.format(
          len(image),selected['flash_base']))
    cpu=machine['sysbus.cpu']
    cpu.VectorTableOffset=selected['flash_base']
    monitor.Parse('sysbus.cpu SP 0x{0:x}'.format(selected['sp']))
    monitor.Parse('sysbus.cpu PC 0x{0:x}'.format(selected['reset']))
    cpu.IsSemihostingEnabled=False
    led=machine['sysbus.gpio.led']
    hook_errors=[]
    def changed(sender,value):
        try:
            print('UEEC_GPIO time={0} level={1}'.format(machine.ElapsedVirtualTime.TimeElapsed,int(value)))
        except Exception as error:
            hook_errors.append(str(error))
    led.StateChanged+=changed
    try:
        monitor.Parse('emulation CreateLEDTester "ueec_led_test" gpio.led')
        period=float(selected['half_period_ms'])/1000.0
        duration=(float(CONFIG['edges'])-0.5)*period
        monitor.Parse('ueec_led_test AssertIsBlinking {0} {1} {1} 0.02'.format(duration,period))
    finally:
        led.StateChanged-=changed
    if hook_errors: raise Exception('LED trace callback failed: '+str(hook_errors))
    print('UEEC_EMULATION_PASS')

try:
    run()
except Exception as error:
    print('UEEC_FAILURE '+str(error))
