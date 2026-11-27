import pathlib, struct, subprocess, sys, tempfile, shlex
arch, exe, runner=sys.argv[1:]
valid=invalid=0
with tempfile.TemporaryDirectory() as d:
    p=pathlib.Path(d)/'metadata.bin'
    def run(blob, endian, expected=None):
        global valid,invalid
        p.write_bytes(blob)
        r=subprocess.run(shlex.split(runner)+[exe,endian,str(p)],capture_output=True,text=True)
        assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
        if expected is None:
            assert r.returncode==1,(r.returncode,r.stdout,r.stderr);invalid+=1
        else:
            assert r.returncode==0 and r.stdout.strip()==expected,(r.returncode,r.stdout,r.stderr,expected);valid+=1
    for end,order in [('le','<'),('be','>')]:
        for pc in (8,12,14,16,17,22,32):
            for sram,eeprom in [(0,0),(512,0),(2048,1024),(8192,4096)]:
                for flags in (0,0x80000000,0xffffffff):
                    a=[min(2**pc*2, 262144),sram,eeprom,64,26,flags,flags^0x12345678]
                    sig=bytes([0x1e,pc,0x55]); blob=struct.pack(order+'HH3sB7I',0,0,sig,pc,*a)
                    run(blob,end,' '.join([sig.hex(),str(pc)]+list(map(str,a))))
        base=struct.pack(order+'HH3sB7I',0,0,b'\x1e\x95\x0f',14,32768,2048,1024,128,26,0,0)
        for n in range(len(base)):run(base[:n],end)
        run(base+b'\x00',end)
        for offset,fmt,values in [(0,'H',[1,65535]),(2,'H',[1,65535]),(4,'3s',[b'\0'*3,b'\xff'*3]),(7,'B',[0,33,13]),(8,'I',[0,32767,65536]),(20,'I',[0,1,130,65536])]:
            for v in values:
                b=bytearray(base);struct.pack_into(order+fmt,b,offset,v);run(b,end)
print(f'[PASS] {arch}: {valid} valid metadata payloads, {invalid} malformed payloads rejected')
