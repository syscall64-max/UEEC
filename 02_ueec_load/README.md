# UEEC loader — ეტაპი 02

ეს პროექტი ტვირთავს ახალი _GM ჰედერის მქონე UEEC v1 ფაილს Linux-ზე.
სატესტო ueec.asm შეიცავს 12 არქიტექტურულ ნოდს და თითოეულისთვის
exit(0) CODE სექციას.

## ჩატვირთვის გზა

1. ფაილი იხსნება open()-ით და მეტამონაცემების ვალიდაციისთვის მთლიანად
   read-only mmap()-ში აისახება.
2. ლოადერი ირჩევს მიმდინარე Linux არქიტექტურის შესაბამის ნოდს.
3. იქმნება შემცირებული runtime HEADER: ერთი ueec_hdr, ერთი
   ueec_node_hdr, არჩეული სექციების ჰედერები, სწორად გასწორებული
   section_address[] და არჩეული ნოდის სახელი.
4. არჩეული სექციები იტვირთება pread()-ით ანონიმურ mmap() არეებში.
5. უფლებები იცვლება mprotect()-ით: HEADER — R, CODE — RX,
   მუდმივი მონაცემები — R, დანარჩენი — RW.
6. პირველი CODE სექციის მისამართი გამოიყენება entry point-ად.

ერთნაირი სახელი სხვადასხვა არქიტექტურის ნოდს შეიძლება ჰქონდეს. ამიტომ
სახელით არჩევის გასაღებია:

    (name, machine_type, machine, system_type, system)

## რეჟიმები

- auto — ჯერ მონოლითური, წარუმატებლობისას გაბნეული;
- monolithic — HEADER და ყველა არჩეული სექცია ერთ ანონიმურ mapping-ში;
- scattered — თითო სექციას ცალკე mapping;
- hybrid — HEADER ცალკე, დანარჩენი არჩეული სექციები ერთ mapping-ში.

ყველა რეჟიმში იქმნება section_address[].

## გამოყენება

    build/ueec-load-x86_64 --node exit build/codes.ueec
    build/ueec-load-x86_64 --node 10 --load-mode scattered build/codes.ueec
    build/ueec-load-x86_64 --node exit --load-mode hybrid --no-run build/codes.ueec

--node-ის გარეშე ავტომატურად აირჩევა მიმდინარე სისტემის პირველი
თავსებადი ნოდი. ინდექსით არჩევისას მითითებული ნოდი ასევე უნდა ემთხვეოდეს
მიმდინარე არქიტექტურასა და სისტემას.

## აგება და ტესტირება

    make all

make all ააწყობს და QEMU-ით გაუშვებს ყველა 12 ვარიანტს:
ARM32, ARM64, LoongArch64, PowerPC32, PowerPC64, RISC-V32, RISC-V64,
SPARC32, SPARC64, x86, x86-64 და IBM Z (s390x).

ცალკეული მიზნების მაგალითები:

    make test-powerpc64
    make test-x86_64
    make test-modes
    make test-s390x
    make test-s390x-modes
    make native

PowerPC64-ზე entry point-ზე გადასვლა ხდება პირდაპირ mtctr/bctr-ით,
რათა raw CODE მისამართი შეცდომით ELFv1 function descriptor-ად არ
იქნეს აღქმული.

## მიმდინარე საზღვრები

- ამ ეტაპზე ყველა ნოდის section-header format ერთნაირი უნდა იყოს.
- auto რეჟიმის direct file-backed mapping ოპტიმიზაცია შემდეგ,
  ერთნოდიან სისტემურ ოპტიმიზატორთან ერთად დაემატება.
- იმპორტი, ექსპორტი და რელოკაცია ამ მინიმალურ CODE ტესტში ჯერ არ მუშავდება.

## IBM Z / s390x (მე-12 არქიტექტურა)

- `UEEC_MACHINE_S390X = 11` — ახალ ნოდს აქვს ინდექსი `11`, `code-endian=big`.
- ერთი საერთო HEADER და 12 CODE სექცია: სულ 13 ფიზიკური სექცია.
- საცდელი CODE: `A7 29 00 00 0A 01 07 07` (8 ბაიტი) —
  `lghi %r2,0; svc 1; bcr 0,7`, Linux `exit(0)`.
- კონტეინერის **მეტამონაცემები little-endian რჩება**, მიუხედავად
  s390x კოდის big-endian ბაიტთა რიგისა.
- განახლებული სატესტო ფაილის მოსალოდნელი ზომაა **399 ბაიტი**.

დამოკიდებულებები Ubuntu-ზე (დანარჩენ 11 არქიტექტურაზე დამოკიდებულებები
ძველებურად საჭიროა სრულ `make all`-სთვის):

```sh
sudo apt install nasm gcc-s390x-linux-gnu qemu-user
make clean
make test-s390x           # რეალური ჩატვირთვა და exit(0) QEMU-ში
make test-s390x-modes     # auto, monolithic, scattered, hybrid, ყველა run
make all                  # მთლიანი 12-არქიტექტურული მატრიცა
```

ეს სატესტო კოდი არ შეიცავს z17-ის სპეციფიკურ ინსტრუქციებს;
შესაბამისად, ამით მოწმდება Linux s390x, არა კონკრეტულად z17 პროცესორი.
