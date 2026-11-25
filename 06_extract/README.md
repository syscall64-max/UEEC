# 06_extract — node extraction

მრავალ-node-იანი UEEC ფაილიდან ერთი node-ის ამოღება, ახალი, **ერთ-node-იანი**
UEEC ფაილის შესაქმნელად.

## შერჩევის პარამეტრები (ნებისმიერი კომბინაცია)

```
--name N            # node-ის სახელი
--machine-type T     # 0=CPU, ...
--machine M          # 0=ARM32, 1=ARM64, ..., 9=X86, 10=X86_64, 11=S390X
--system-type ST      # 2=GENERAL_OS, ...
--system S            # 2=LINUX, ...
```

**ქცევა:**
- **ზუსტად ერთი** node ემთხვევა → ამოღება წარმატებით სრულდება
- **ბუნდოვანება** (რამდენიმე node ემთხვევა) → ცხადი შეცდომა, ემთხვეული
  node-ების id-ების ჩამონათვალით — არ ირჩევს "პირველს" ჩუმად
- **არცერთი არ ემთხვევა** → ცხადი შეცდომა

## გაშვება

```
./extract multi.ueec node.ueec --machine 10
./extract multi.ueec node.ueec --machine-type 0 --machine 9   # კომბინაცია
./extract multi.ueec node.ueec --name hello                    # თუ node-ებს განსხვავებული სახელები აქვთ
```

```
make run-x86_64      # x86-64: extract + ორივე load-mode
make run-combo        # x86: --machine-type+--machine კომბინაცია
make run-ambiguous    # განზრახ ბუნდოვანი selector (უნდა ჩავარდეს, ცხადი შეცდომით)
make all               # 12 architecture (QEMU-ს ქვეშ; x86-64 native)
make -k all            # თუ /opt/riscv32 არ გაქვთ
```

**12-არქიტექტურული ტესტის მიზანი: 24/24 (12 architecture × 2 load-mode)** — ორივე extract-ისა
(სწორი node-ის ცალსახა ამორჩევა) და loader-ის (ამოღებული, ერთ-node-იანი
ფაილის ჩატვირთვა/გაშვება) სისწორე.

## ტექნიკური დეტალი

RELOC-სექციის `target`/`source` (node-local ინდექსები) **უცვლელი** რჩება
extraction-ისას — ეს გადაწყვეტილება მუშაობს, ვინაიდან ეს ინდექსები **უკვე**
node-ის საკუთარ, resident სექციებზეა შეფარდებითი (0-დან დაწყებული), არა
გლობალურ section-index-ზე — ამოღების დროს მხოლოდ **გლობალური** section-table
(`node_info`, `sh16[]`) ეხატება ხელახლა.

## ცნობილი შეზღუდვები

`riscv32` საჭიროებს `/opt/riscv32` toolchain-ს (`riscv32-unknown-linux-gnu-gcc`).

## S390X / IBM Z — დამატება (12-ე არქიტექტურა)

- მანქანის კოდი: `UEEC_MACHINE_S390X = 11`.
- `multi.ueec`: 12 ნოდი, 26 სექცია, 1116 ბაიტი (კომპაქტური საცდელი ფაილი).
- S390X-ის `CODE` შეიცავს 40 ბაიტს; `RELOCATION` იყენებს `ABS64`-ს (offset `0x20`).
- `extract` გამოყოფს საერთო DATA სექციის ასლს, არჩეულ CODE-სა და RELOCATION-ს;
  თვით ფიზიკური გაზიარება ერთნოდიან ფაილში აღარ არსებობს.
- არჩეულ ფაილში ნოდის ახალი ინდექსია 0; რელოკაციის local[0]/local[1] ინდექსები უცვლელია.
- ამ ვერსიის extractor მუშაობს little-endian ჰოსტზე და SH16 კომპაქტურ
  საწყის ფაილებზე. ეს არ წარმოადგენს ზოგად უსაფრთხო UEEC parser-ს.

```sh
make clean
make check-all-layout
make s390x
make all
```

`make s390x` გამოყოფს `node_s390x.ueec` ფაილს და უშვებს მის კოდს
`monolithic` და `scattered` რეჟიმებში QEMU-S390X-ით.
`check_extracted.py` ადარებს გამოყოფილ CODE/DATA/RELOCATION ბაიტებს საწყის
კონტეინერთან; ამოწმებს ნოდის იდენტიფიკატორს, სექციების ტიპებსა და
ლოკალურ რელოკაციის ინდექსებს.

ნომერი **11** შენარჩუნებულია სხვა ეტაპების შესაბამისი კონვენციით.
