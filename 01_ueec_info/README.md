# 01 — ueec-info

პროექტი ქმნის ერთ `codes.ueec` ფაილს, რომელშიც არის 11 Linux/CPU ნოდი,
ერთი საერთო HEADER სექცია და 11 CODE სექცია. `ueec-info` ფაილს `open` +
`mmap` გზით კითხულობს, ამოწმებს wire layout-ს და ბეჭდავს ჰედერის, ნოდების,
სახელების, mapping-ებისა და სექციების ინფორმაციას.

## ახალი layout

- `ueec_hdr`: 8 ბაიტი, `_GM`, version/endian, `nodes`, `names`;
- `ueec_node_hdr`: 6 ბაიტი, მათ შორის 16-ბიტიანი section format;
- `ueec_node_info`: 6 ბაიტი (`uint16_t node`, `uint32_t section`);
- HEADER არის სავალდებულო გლობალური `section 0`;
- HEADER ყველა ნოდს implicit-ად ეკუთვნის და `ueec_node_info`-ში არ მეორდება;
- სახელების განლაგებაა `name_len[]`, `node_name_index[]`, `name_data[]`;
- 11 ნოდი ერთ საერთო ლოგიკურ სახელს — `exit` — იყენებს;
- სექციის ჰედერებია `SH16`, `SH32` ან `SH64`.

## აწყობა და ტესტირება

```sh
make ueec                 # მხოლოდ codes.ueec
make build-x86_64         # ერთი არქიტექტურის ueec-info
make test-x86_64          # ერთი არქიტექტურის ტესტი
make build                # 11 არქიტექტურის კომპილაცია
make test                 # 11 არქიტექტურის გაშვება/QEMU ტესტი
make all                  # იგივე, რაც make test
make native               # x86-64-ის სწრაფი ლოკალური ტესტი
```

საჭიროა NASM, შესაბამისი cross-compiler-ები და QEMU user-mode emulator-ები.
სტატიკური კომპილაციის გამო target sysroot-ის მითითება ჩვეულებრივ საჭირო არ არის.

## მიმდინარე შეზღუდვა

ინფორმაციის უტილიტა ამ ეტაპზე მოითხოვს, რომ ყველა ნოდი ერთსა და იმავე section
header format-ს იყენებდეს. სხვადასხვა ფორმატის ნოდების ერთ ფაილში ფიზიკური wire
encoding ჯერ საბოლოოდ არ არის განსაზღვრული, ამიტომ parser ასეთ ფაილს მკაფიო
შეცდომით უარყოფს.
