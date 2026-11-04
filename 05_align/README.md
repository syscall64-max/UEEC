# 05_align — გრანულაციის მართვა

`align` ხელსაწყო კითხულობს UEEC ფაილს და თითოეული სექციის
`section_size`-ს (padding-ითურთ) აფართოებს granularity-ის ჯერადამდე —
`data_size` (რეალური შემცველობა) **უცვლელი** რჩება.

## გაშვება

```
./align input.ueec output.ueec                    # ნაგულისხმევი: sysconf(_SC_PAGESIZE)
./align input.ueec output.ueec --granularity 1     # section_size == data_size (padding-ის გარეშე)
./align input.ueec output.ueec --granularity 64    # ნებისმიერი, ცხადი granularity
```

**`loader.c`-ს (ორივე, `03_reloc`-ის) ცვლილება დასჭირდა** — თავდაპირველად
`section_size`-სა და `data_size`-ს ერთმანეთისგან არ განასხვავებდა (ორივესთვის
ერთსა და იმავე მნიშვნელობას იყენებდა). ახლა `data_size`-ს სწორად კითხულობს
(თუმცა ამ loader-ში ცალკე გამოყენებული არ არის — `section_size`-ით
memcpy-ვა უკვე საკმარისია, ვინაიდან `align`-ის მიერ დამატებული padding
**ფაილშივეა** ჩაწერილი, ნულოვანი ბაიტებით).

## გაშვება — loader ორივე ფაილზე

```
make run-default    # x86-64, sysconf-granularity-ით padded ფაილი
make run-g1          # x86-64, granularity=1 (padding-ის გარეშე)
make all             # ყველა architecture, ორივე ფაილი, ორივე load-mode
make -k all          # თუ /opt/riscv32 არ გაქვთ
```

**48 სცენარი (12 არქიტექტურა × 2 ფაილი × 2 load-mode) — შესამოწმებელია Ubuntu/QEMU-ში.**

## რეალური ზომები (ამ ტესტიდან)

- `test.ueec` (`03_reloc`-ის ორიგინალი, padding-ის გარეშე): `1116` ბაიტი
- `test_default.ueec` (`4096`-granularity): `106496` ბაიტი (`26` სექცია × `4096`)
- `test_g1.ueec` (`--granularity 1`): `1116` ბაიტი — **ბაიტობრივად იდენტური**
  ორიგინალთან (ვინაიდან ორიგინალშიც `section_size == data_size` იყო ყველგან)

## ცნობილი შეზღუდვები

`riscv32` საჭიროებს `/opt/riscv32` toolchain-ს (`riscv32-unknown-linux-gnu-gcc`).


## S390X (IBM Z) — ახალი მეთორმეტე არქიტექტურა

UEEC node ID `11`, Big-Endian CODE და ABS64 რელოკაცია. `03_reloc.asm`
განახლებულია უკვე შემოწმებული `03_reloc_12arch_s390x` ეტაპიდან.
ამ ეტაპის `align` კი მხოლოდ little-endian UEEC wire metadata-ს ამუშავებს;
S390X-ზე cross-compiled არის loader, ხოლო align უტილიტა x86-64 ჰოსტზე სრულდება.

```sh
make clean
make test-layout test-compact
make s390x     # QEMU: default/granularity=1 × monolithic/scattered
make all       # 12 architecture × 2 layouts × 2 load modes
```

`make all`-ის ყველა 48 შესრულება ჯერ მომხმარებლის Ubuntu-ზე უნდა დადასტურდეს.
ტესტები არ ამტკიცებენ ზუსტ z17 ISA ინსტრუქციების მხარდაჭერას.
