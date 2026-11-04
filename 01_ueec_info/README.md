# 01 — ueec-info

პროექტი ქმნის ერთ `codes.ueec` ფაილს, რომელშიც არის 12 Linux/CPU ნოდი,
ერთი საერთო HEADER სექცია და 12 CODE სექცია. `ueec-info` ფაილს `open` +
`mmap` გზით კითხულობს, ამოწმებს wire layout-ს და ბეჭდავს ჰედერის, ნოდების,
სახელების, mapping-ებისა და სექციების ინფორმაციას.

## ახალი layout

- `ueec_hdr`: 8 ბაიტი, `_GM`, version/endian, `nodes`, `names`;
- `ueec_node_hdr`: 6 ბაიტი, მათ შორის 16-ბიტიანი section format;
- `ueec_node_info`: 6 ბაიტი (`uint16_t node`, `uint32_t section`);
- HEADER არის სავალდებულო გლობალური `section 0`;
- HEADER ყველა ნოდს implicit-ად ეკუთვნის და `ueec_node_info`-ში არ მეორდება;
- სახელების განლაგებაა `name_len[]`, `node_name_index[]`, `name_data[]`;
- 12 ნოდი ერთ საერთო ლოგიკურ სახელს — `exit` — იყენებს;
- სექციის ჰედერებია `SH16`, `SH32` ან `SH64`.

## აწყობა და ტესტირება

```sh
make ueec                 # მხოლოდ codes.ueec
make build-x86_64         # ერთი არქიტექტურის ueec-info
make test-x86_64          # ერთი არქიტექტურის ტესტი
make build                # 12 არქიტექტურის კომპილაცია
make test                 # 12 არქიტექტურის გაშვება/QEMU ტესტი
make all                  # იგივე, რაც make test
make native               # x86-64-ის სწრაფი ლოკალური ტესტი
make test-s390x           # S390X parser-ის გაშვება QEMU-ში
make test-layout          # 12 ნოდის და S390X-ის metadata შემოწმება
```

საჭიროა NASM, შესაბამისი cross-compiler-ები და QEMU user-mode emulator-ები.
სტატიკური კომპილაციის გამო target sysroot-ის მითითება ჩვეულებრივ საჭირო არ არის.

## მიმდინარე შეზღუდვა

ინფორმაციის უტილიტა ამ ეტაპზე მოითხოვს, რომ ყველა ნოდი ერთსა და იმავე section
header format-ს იყენებდეს. სხვადასხვა ფორმატის ნოდების ერთ ფაილში ფიზიკური wire
encoding ჯერ საბოლოოდ არ არის განსაზღვრული, ამიტომ parser ასეთ ფაილს მკაფიო
შეცდომით უარყოფს.

## IBM Z / S390X დამატება (მე-12 არქიტექტურა)

- არქიტექტურის ID: `UEEC_MACHINE_S390X = 11` (ადრინდელი ID-ები `0..10` უცვლელია).
- ნოდი: ინდექსი `11`, `CPU/s390x`, Linux, Big-Endian მანქანური კოდი.
- `ueec_node_info`-ში ახალი კავშირია `node=11 → section=12`.
- ერთი საერთო HEADER და 12 CODE სექცია. საერთო სახელია `exit`.
- `ueec.asm`-ში S390X-ის ბაიტებია: `A7 29 00 00 0A 01 07 07`,
  მიღებული `s390x-linux-gnu-objcopy -O binary -j .text` ბრძანებით:
  `lghi %r2,0; svc 1` და ბოლოში `bcr 0,7` (NOP).
- კონტეინერის **metadata Little-Endian** რჩება, ხოლო S390X ნოდის `code-endian=big`.

Ubuntu-ზე (ინსტრუმენტების არსებობისას):

```sh
make clean
make ueec
make test-s390x
make test-layout
make all
```

S390X-ის ხელსაწყოები: `s390x-linux-gnu-gcc`, `qemu-s390x`; ასევე საჭიროა `nasm`.
გაითვალისწინე: ამ `01_ueec_info` ეტაპზე QEMU-ში **პარსერი** მუშაობს და
ნოდებს/სექციებს ამოწმებს, მაგრამ `CODE` სექციის კოდს **არ ასრულებს**.
UEEC loader-ით S390X CODE-ის შესრულება `02_ueec_load` ეტაპზე შესამოწმებელია.

### მინიმალური შემოწმებები

```sh
make native | grep -E 'nodes:|global sections:|machine=CPU/s390x'
# უნდა გამოჩნდეს nodes: 12; global sections: 13; CPU/s390x ... code-endian=big
```
