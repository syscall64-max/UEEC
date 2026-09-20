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

**40/40 (10 architecture × 2 ფაილი × 2 load-mode) დამოწმებული.**

## რეალური ზომები (ამ ტესტიდან)

- `test.ueec` (`03_reloc`-ის ორიგინალი, padding-ის გარეშე): `1028` ბაიტი
- `test_default.ueec` (`4096`-granularity): `98304` ბაიტი (`24` სექცია × `4096`)
- `test_g1.ueec` (`--granularity 1`): `1028` ბაიტი — **ბაიტობრივად იდენტური**
  ორიგინალთან (ვინაიდან ორიგინალშიც `section_size == data_size` იყო ყველგან)

## ცნობილი შეზღუდვები

`riscv32` საჭიროებს `/opt/riscv32` toolchain-ს (`riscv32-unknown-linux-gnu-gcc`).
