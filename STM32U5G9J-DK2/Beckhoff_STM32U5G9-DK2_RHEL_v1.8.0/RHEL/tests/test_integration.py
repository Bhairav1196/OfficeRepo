#!/usr/bin/env python3
"""Real gateway with one combined loopback EIP simulator. Checks the v1.8 wire
contract, not ODVA conformance. Does not access any physical coupler."""
import concurrent.futures as futures
import contextlib
import pathlib
import signal
import socket
import struct as st
import subprocess
import sys
import tempfile
import threading
import time


def exact(s, n):
    data=b''
    while len(data)<n:
        part=s.recv(n-len(data))
        if not part: raise EOFError()
        data+=part
    return data


def port(kind=socket.SOCK_STREAM):
    with socket.socket(socket.AF_INET,kind) as s:
        s.bind(('127.0.0.1',0)); return s.getsockname()[1]


def wait(check, timeout=5):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if check(): return
        time.sleep(.02)
    raise AssertionError('condition timed out')


class Coupler:
    def __init__(self, unit):
        self.unit=unit; self.ip='127.0.0.'+str(unit+1)
        self.input=bytearray(30)
        self.output=bytearray(22)
        st.pack_into('<H',self.input,4,0x1234)
        st.pack_into('<H',self.output,4,0x40)
        for ch,raw in enumerate([30518,-30518,15259,0]):
            st.pack_into('<Hh',self.input,6+4*ch,ch,raw)
        for ch in range(4): st.pack_into('<H',self.input,22+2*ch,10+ch)
        self.tcp=socket.socket(); self.tcp.bind((self.ip,0)); self.tcp.listen(); self.tcp.settimeout(.1)
        self.port=self.tcp.getsockname()[1]
        self.udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.udp.bind((self.ip,0));self.udp.settimeout(.1);self.udp_port=self.udp.getsockname()[1]
        self.stop=threading.Event(); self.open=False;self.mode='normal'
        self.bad_size=False;self.bad_context=False;self.errors=[];self.opens=0
        self.seq=0xfffffff0;self.o2t=0;self.t2o=0
        self.threads=[threading.Thread(target=self.accept,daemon=True),threading.Thread(target=self.cyclic,daemon=True)]
        for t in self.threads:t.start()

    def accept(self):
        while not self.stop.is_set():
            try:s,_=self.tcp.accept()
            except socket.timeout:continue
            except OSError:break
            t=threading.Thread(target=self.session,args=(s,),daemon=True);self.threads.append(t);t.start()

    def session(self,s):
        s.settimeout(1)
        try:
            with s:
                while not self.stop.is_set():
                    try:hdr=exact(s,24)
                    except socket.timeout:continue
                    cmd,n,handle,_,ctx,_=st.unpack('<HHII8sI',hdr);payload=exact(s,n)
                    if cmd==0x65:
                        assert payload==b'\1\0\0\0';handle=0x12340000+self.unit;out=payload
                    elif cmd in (0,0x66):
                        if cmd==0x66:break
                        continue
                    elif cmd==0x6f:
                        assert handle==0x12340000+self.unit
                        cip=payload[16:];svc=cip[0];extra=b''
                        if svc==0x0e:
                            data=bytes(self.input if cip[5]==129 else self.output)
                            if self.bad_size and cip[5]==129:data+=b'\0'
                        elif svc==0x54:
                            assert cip[40]==1,('transport class/cyclic',cip[40])
                            assert cip[-8:]==bytes([32,4,36,128,44,130,44,129])
                            assert st.unpack_from('<H',cip,32)[0]==0x4800|(len(self.output)+6)
                            assert st.unpack_from('<H',cip,38)[0]==0x4800|(len(self.input)+2)
                            self.o2t=0x20000000+self.unit;self.t2o=st.unpack_from('<I',cip,12)[0]
                            self.opens+=1;self.seq=0xfffffff0
                            data=st.pack('<II',self.o2t,self.t2o)+cip[16:24]+st.pack('<IIBB',10000,10000,0,0)
                            sockaddr=st.pack('>HH4s8s',2,self.udp_port,socket.inet_aton(self.ip),b'\0'*8)
                            extra=st.pack('<HH',0x8000,16)+sockaddr;self.open=True
                        elif svc==0x4e:self.open=False;data=cip[8:16]+b'\0\0'
                        else:raise AssertionError(('service',svc))
                        reply=bytes([svc|128,0,0,0])+data
                        out=st.pack('<IHHHHHH',0,0,3 if extra else 2,0,0,0xb2,len(reply))+reply+extra
                    else:raise AssertionError(('command',cmd))
                    if self.bad_context:ctx=b'BADCTXT!'
                    reply=st.pack('<HHII8sI',cmd,len(out),handle,0,ctx,0)+out
                    s.sendall(reply[:11]);s.sendall(reply[11:])
        except (EOFError,ConnectionError,socket.timeout,OSError):pass
        except Exception as e:self.errors.append(repr(e))

    def cyclic(self):
        while not self.stop.is_set():
            try:
                packet,peer=self.udp.recvfrom(2048)
                if not self.open:continue
                assert st.unpack_from('<HHH',packet)==(2,0x8002,8)
                assert st.unpack_from('<I',packet,6)[0]==self.o2t
                assert st.unpack_from('<HH',packet,14)==(0xb1,len(self.output)+6)
                assert st.unpack_from('<I',packet,20)[0]==1
                assert len(packet)==len(self.output)+24
                self.output[:]=packet[24:]
                if self.mode=='silent':continue
                if self.mode!='replay':self.seq=(self.seq+1)&0xffffffff
                addr=st.pack('<HHHII',2,0x8002,8,self.t2o,self.seq)
                data=st.pack('<H',self.seq&65535)+self.input
                reply=addr+st.pack('<HH',0xb1,len(data))+data
                if self.mode=='trailing':reply+=b'bad'
                self.udp.sendto(reply,peer)
            except socket.timeout:continue
            except OSError:break
            except Exception as e:self.errors.append(repr(e))

    def close(self):
        self.stop.set();self.tcp.close();self.udp.close()
        for t in self.threads:t.join(1.2)


class Client:
    def __init__(self,p,ip='127.0.0.1'):
        self.sock=socket.socket();self.sock.settimeout(2);self.sock.bind((ip,0));self.sock.connect(('127.0.0.1',p));self.tid=0
    def frame(self,u,pdu):
        self.tid=(self.tid+1)&65535
        return st.pack('>HHHB',self.tid,0,len(pdu)+1,u)+pdu
    def receive(self,frame):
        h=exact(self.sock,7);assert h[:4]==frame[:4] and h[6]==frame[6]
        return exact(self.sock,st.unpack_from('>H',h,4)[0]-1)
    def request(self,u,pdu,fragment=False):
        f=self.frame(u,pdu)
        if fragment:
            for byte in f:self.sock.sendall(bytes([byte]))
        else:self.sock.sendall(f)
        return self.receive(f)
    def read(self,u,fc,a,q):return self.request(u,st.pack('>BHH',fc,a,q))
    def close(self):self.sock.close()


def main():
    exe=pathlib.Path(sys.argv[1]).resolve();root=pathlib.Path(__file__).resolve().parents[1]
    devices=[Coupler(1)];clients=[];process=None;checks=[]
    def passed(s):checks.append(s);print('PASS:',s,flush=True)
    temp_object=tempfile.TemporaryDirectory(prefix='ek9500-test-');temp=pathlib.Path(temp_object.name)
    try:
        ports=[port(),port()];local_udp=port(socket.SOCK_DGRAM)
        (temp/'masters.conf').write_text('Host|127.0.0.1|12|10000\nRateTest|127.0.0.5|1|3\n')
        for name,d in zip(['coupler.conf'],devices):
            text=(root/'config'/name).read_text().replace('192.168.1.1',d.ip)
            text=text.replace('tcp_port = 44818',f'tcp_port = {d.port}').replace('target_udp_port = 2222',f'target_udp_port = {d.udp_port}')
            text=text.replace('local_udp_port = 2222',f'local_udp_port = {local_udp}').replace('reconnect_delay_ms = 1000','reconnect_delay_ms = 100')
            (temp/name).write_text(text)
        conf=(root/'config/gateway.conf').read_text().replace('port = 1502',f'port = {ports[0]}').replace('secondary_port = 502',f'secondary_port = {ports[1]}')
        conf=conf.replace('bind_ip = 0.0.0.0','bind_ip = 127.0.0.1');(temp/'gateway.conf').write_text(conf)
        log=open(temp/'gateway.log','w+');process=subprocess.Popen([str(exe),str(temp/'gateway.conf')],stdout=log,stderr=log)
        def connect():
            try:clients.append(Client(ports[0]));return True
            except ConnectionError:
                assert process.poll() is None,(temp/'gateway.log').read_text();return False
        wait(connect);c=clients[-1]
        wait(lambda:c.read(1,2,0,16)==b'\2\2\x34\x12')
        assert c.read(1,4,1024,4)==b'\4\10'+st.pack('>hhhh',10000,-10000,5000,0)
        assert c.read(1,4,3072,4)==b'\4\10'+st.pack('>HHHH',10,11,12,13)
        assert c.read(1,1,0,16)==b'\1\2\x40\0'
        passed('combined Class-1 route; wire sizes, preserved startup DO, AI/AO-status mapping')
        assert c.request(1,st.pack('>BHH',6,0,132),True)==st.pack('>BHH',6,0,132)
        wait(lambda:abs(st.unpack_from('<f',devices[0].output,6)[0]-.00132)<1e-9)
        assert c.read(1,3,0,1)==b'\3\2\0\x84'
        passed('fragmented Modbus; 132 -> 1.32 mA -> 0.00132 A in cyclic output')
        before=c.read(1,3,0,4)
        assert c.request(1,st.pack('>BHHBhh',16,0,2,4,899,2001))==b'\x90\3'
        assert c.read(1,3,0,4)==before
        assert c.request(1,st.pack('>BHHHHBh',23,4,1,0,1,2,999))==b'\x97\2'
        assert c.read(1,3,0,4)==before
        assert c.request(1,st.pack('>BHHHHBhh',23,0,2,0,2,4,-500,899))==b'\x17\4'+st.pack('>hh',-500,899)
        passed('atomic FC16; FC23 rejects invalid read range before writing; correct read-after-write')
        for a in [0,5,15]:
            pdu=st.pack('>BHH',5,a,0xff00);assert c.request(1,pdu)==pdu
        assert c.request(1,st.pack('>BHHBB',15,8,4,1,5))==st.pack('>BHH',15,8,4)
        assert c.read(1,1,0,16)==b'\1\2'+st.pack('<H',0x8561)
        passed('FC05 and FC15 preserve unrelated DO bits')
        frames=[c.frame(1,st.pack('>BHH',2,0,16)) for _ in range(40)]
        c.sock.sendall(b''.join(frames));assert all(c.receive(f)==b'\2\2\x34\x12' for f in frames)
        passed('40 coalesced ADUs exceed one event budget without stalling')
        c2=Client(ports[1]);clients.append(c2);assert c2.read(1,2,0,16)[0]==2
        r=Client(ports[0],'127.0.0.5');clients.append(r)
        for _ in range(3):assert r.read(1,3,0,1)[0]==3
        before=c.read(1,3,0,1);assert r.request(1,st.pack('>BHH',6,0,777))==b'\x86\6'
        assert c.read(1,3,0,1)==before;time.sleep(1.02);assert r.read(1,3,0,1)==before
        passed('dual listeners; over-limit write has no effect; same TCP connection recovers')
        source_port=c.sock.getsockname()[1]
        for mode in ['silent','replay','trailing']:
            devices[0].mode=mode;wait(lambda:c.read(1,2,0,16)==b'\x82\6')
            devices[0].mode='normal';wait(lambda:c.read(1,2,0,16)[0]==2)
            assert c.sock.getsockname()[1]==source_port
        passed('drop/replay/malformed UDP rejected; freshness timeout and recovery')
        devices[0].mode='silent';devices[0].bad_size=True
        wait(lambda:c.read(1,2,0,16)==b'\x82\6');opens=devices[0].opens
        time.sleep(.35);assert devices[0].opens==opens
        devices[0].bad_size=False;devices[0].bad_context=True;time.sleep(.3)
        assert c.read(1,2,0,16)==b'\x82\6'
        devices[0].bad_context=False;devices[0].mode='normal';wait(lambda:c.read(1,2,0,16)[0]==2)
        passed('assembly mismatch and wrong sender context fail closed; automatic recovery')
        other=subprocess.run([str(exe),str(temp/'gateway.conf')],capture_output=True,timeout=5)
        assert other.returncode!=0 and b'Cannot lock PID' in other.stderr
        passed('duplicate instance rejected before device access')
        jobs=[(1,2,0,16),(1,1,0,16),(1,4,1024,4),(1,4,3072,4),(1,3,0,4)]
        latency=[];start=time.perf_counter()
        for index in range(3150):
            remaining=start+index/315-time.perf_counter()
            if remaining>0:time.sleep(remaining)
            then=time.perf_counter();assert not c.read(*jobs[index % len(jobs)])[0]&128
            latency.append((time.perf_counter()-then)*1000)
        a=sorted(latency)
        passed(f'3150 polls/10 s, zero exceptions; loopback p50={a[1575]:.3f} ms p99={a[3118]:.3f} ms max={max(a):.3f} ms')
        def load(unit):
            with contextlib.closing(Client(ports[0])) as q:
                for _ in range(500):assert q.read(unit,2,0,16)[0]==2
        with futures.ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(load,[1,1,1,1]))
        passed('four simultaneous masters, 2000 requests, zero exceptions')
        for d in devices:assert not d.errors,d.errors
        process.send_signal(signal.SIGTERM);assert process.wait(8)==0
        passed('graceful shutdown with the combined I/O route and persistent clients')
        print(f'INTEGRATION: {len(checks)} groups passed',flush=True)
    except Exception:
        print((temp/'gateway.log').read_text()[-5000:],file=sys.stderr)
        raise
    finally:
        for c in clients:c.close()
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(8)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        for d in devices:
            d.close()
            if d.errors:print(d.errors,file=sys.stderr)
        temp_object.cleanup()


if __name__=='__main__':main()
