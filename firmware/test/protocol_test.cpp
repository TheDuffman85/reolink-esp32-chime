#include "Protocol.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace chime;
using Bytes = std::vector<uint8_t>;

// Fixed synthetic fixture, not a captured Reolink transmission. CRC 4AA2
// was computed independently by polynomial long division. Ciphertext is
// AES-128(zero key, zero counter) XOR 09 AB 34 12, known zero-block vector.
const Bytes body = {0x14,0x14,0x03,0x34,0x12,0xD4,0xC3,0xB2,0xA1,
                    0x44,0x33,0x22,0x11,0x04,0x03,0x02,0x01,0x6F,0x42,0x7F,0xC6,0x4A,0xA2};
const Bytes wire = {0xEB,0xF5,0x1E,0xAE,0xFF,0x51,0xF0,0x96,0x4B,0x3E,
                    0xE1,0x1B,0x61,0x93,0x54,0x08,0x55,0x12,0x6F,0xA7,0xAB,0x47,0x18};

Bytes frame(const Bytes &whitened) {
  Bytes result = {0x55,0x55,0x55,0x55,0x93,0x0B,0x51,0xDE};
  result.insert(result.end(),whitened.begin(),whitened.end()); return result;
}
std::vector<Packet> feed(Decoder &decoder, const Bytes &bytes, bool inverted=false, unsigned offset=0) {
  std::vector<Packet> result;
  Packet packet;
  for (unsigned i=0;i<offset;++i) decoder.feedBit(i&1,packet);
  for (uint8_t byte:bytes) for(int bit=7;bit>=0;--bit)
    if(decoder.feedBit(((byte>>bit)&1)^inverted,packet)) result.push_back(packet);
  return result;
}
Bytes whiten(Bytes bytes) {
  uint16_t pn9=0x1FF; for(auto &byte:bytes) byte ^= pn9Mask(pn9); return bytes;
}
int main() {
  uint16_t pn9=0x1FF;
  for(uint8_t expected:Bytes{0xFF,0xE1,0x1D,0x9A,0xED,0x85,0x33,0x24}) assert(pn9Mask(pn9)==expected);
  const auto *digits=reinterpret_cast<const uint8_t *>("123456789");
  assert(crc16(digits,9,0x8005)==0xAEE7);
  assert(crc16(digits,9,0x1021)==0x29B1);
  assert(crc16(body.data(),body.size()-2,0x8005)==0x4AA2);
  assert(whiten(body)==wire);
  for(bool inverted:{false,true}) for(unsigned offset=0;offset<8;++offset) {
    Decoder decoder;
    const auto packets=feed(decoder,frame(wire),inverted,offset);
    assert(packets.size()==1);
    const auto &packet=packets[0];
    assert(packet.inverted==inverted && packet.size==23 && packet.crc==Crc::Ti8005);
    assert(std::memcmp(packet.bytes,body.data(),body.size())==0);
    Ring ring;
    assert(ringHeader(packet,ring));
    assert(ring.pair==0x1234 && ring.destination==0xA1B2C3D4);
    assert(ring.source==0x11223344 && ring.sequence==0x01020304);
    const uint8_t plain[]={9,0xAB,0x34,0x12};
    assert(ringPayload(plain,ring) && ring.parameter==0x1234);
    uint8_t data[4];
    assert(dataPayload(packet,0xA1B2C3D4,0x11223344,0x1234,data,sizeof(data))==4);
    assert(std::memcmp(data,body.data()+17,4)==0);
    assert(!dataPayload(packet,0xA1B2C3D4,0x11223344,0x1235,data,sizeof(data)));
    assert(!dataPayload(packet,0x11223344,0xA1B2C3D4,0x1234,data,sizeof(data)));
    assert(!dataPayload(packet,0xA1B2C3D4,0x11223344,0x1234,data,3));
  }
  Decoder decoder;
  decoder.setPolarity(Polarity::Normal);
  assert(feed(decoder,frame(wire),true).empty());
  assert(feed(decoder,frame(wire)).size()==1);
  decoder.setPolarity(Polarity::Inverted);
  assert(feed(decoder,frame(wire)).empty());
  assert(feed(decoder,frame(wire),true).size()==1);
  decoder.setPolarity(Polarity::Auto);
  auto damaged=wire; damaged[17]^=1;
  auto bad=feed(decoder,frame(damaged));
  assert(bad.size()==1 && bad[0].crc==Crc::Bad);
  Ring ring;
  assert(!ringHeader(bad[0],ring));
  Bytes fallback=body;
  uint16_t crc=crc16(fallback.data(),fallback.size()-2,0x1021);
  fallback[fallback.size()-2]=crc>>8; fallback.back()=crc;
  auto diagnostic=feed(decoder,frame(whiten(fallback)));
  assert(diagnostic.size()==1 && diagnostic[0].crc==Crc::CcittDiagnostic);
  assert(!ringHeader(diagnostic[0],ring));
  for(uint8_t invalid:{uint8_t(0),uint8_t(1),uint8_t(97),uint8_t(255)}) {
    assert(feed(decoder,frame(Bytes{uint8_t(invalid^0xFF)})).empty());
    assert(feed(decoder,frame(wire)).size()==1);
  }
  assert(decoder.invalidLengths==4);
  // Queue discontinuities reset partial frames, then arbitrary alignment recovers.
  assert(feed(decoder,Bytes{0x93,0x0B,0x51,0xDE,wire[0],wire[1]}).empty());
  decoder.reset(); assert(feed(decoder,frame(wire),false,3).size()==1);
  auto packet=feed(decoder,frame(wire))[0];
  packet.bytes[1]=19; assert(!ringHeader(packet,ring));
  packet.bytes[1]=20; packet.bytes[2]=2; assert(!ringHeader(packet,ring));
  uint8_t wrongOpcode[]={8,0,0,0}; assert(!ringPayload(wrongOpcode,ring));
  // Upper type flag bits don't change the type-3 demultiplexing.
  packet.bytes[2]=0xC3; assert(ringHeader(packet,ring));
  Duplicates duplicates;
  assert(!duplicates.seen(ring,100)); assert(duplicates.seen(ring,101));
  auto other=ring; ++other.sequence;
  assert(!duplicates.seen(other,102)); assert(duplicates.seen(ring,103));
  other=ring; ++other.source; assert(!duplicates.seen(other,104));
  other=ring; ++other.pair; assert(!duplicates.seen(other,105));
  assert(!duplicates.seen(ring,300100));
  duplicates.reset(); assert(!duplicates.seen(ring,UINT32_MAX-100));
  assert(duplicates.seen(ring,100));
  duplicates.reset(); assert(!duplicates.seen(ring,100));
  // Bound parser memory under continuous noisy data with occasional exact sync.
  uint32_t random=0xA3B1C2D3;
  for(int i=0;i<1000000;++i) {
    random^=random<<13; random^=random>>17; random^=random<<5;
    Packet noise;
    if(decoder.feedBit(random&1,noise)) assert(noise.size<=MaxMac+3);
    if(i%10000==0) { decoder.reset(); assert(feed(decoder,frame(wire),true,i%8).size()==1); }
  }
  // Three copies, each damaged in a different bit, vote back to the frame.
  {
    Packet good; good.size=body.size(); std::memcpy(good.bytes,body.data(),body.size()); good.crc=Crc::Ti8005;
    const auto damaged=[&](size_t byte,uint8_t mask) { Packet p=good; p.bytes[byte]^=mask; p.crc=Crc::Bad; return p; };
    Combiner combiner; Packet out;
    assert(!combiner.offer(damaged(5,0x01),0,out));
    assert(!combiner.offer(damaged(9,0x80),100,out));
    assert(combiner.offer(damaged(20,0x10),200,out));
    assert(out.crc==Crc::Ti8005 && out.size==good.size && std::memcmp(out.bytes,good.bytes,good.size)==0);
    // Used copies are gone; the same bit damaged twice cannot be restored.
    assert(!combiner.offer(damaged(5,0x01),300,out));
    assert(!combiner.offer(damaged(5,0x01),400,out) && !combiner.offer(damaged(9,0x80),500,out));
    // Copies older than three seconds, of another length, or with a valid
    // frame in between take no part.
    combiner.reset();
    assert(!combiner.offer(damaged(5,0x01),0,out) && !combiner.offer(damaged(9,0x80),100,out));
    assert(!combiner.offer(damaged(20,0x10),3101,out));
    combiner.reset();
    assert(!combiner.offer(damaged(5,0x01),0,out) && !combiner.offer(damaged(9,0x80),100,out));
    combiner.drop(good.size); assert(!combiner.offer(damaged(20,0x10),200,out));
    Packet shorter=damaged(3,0x02); shorter.size=10; shorter.bytes[0]=7;
    combiner.reset();
    assert(!combiner.offer(damaged(5,0x01),0,out) && !combiner.offer(shorter,50,out));
    assert(!combiner.offer(damaged(20,0x10),100,out));
  }
  std::cout << "Protocol tests passed: vectors, polarity/alignment, CRC gating, recovery, duplicate rollover, noise, "
               "majority restore\n";
}
