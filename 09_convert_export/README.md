# 09_convert_export — უნივერსალური EXPORT-კონვერტერი (COMPACT_GROUP ↔ SIMPLE_FLAT)

`04_convert`-ის ანალოგიური, უნივერსალური კონვერტერი — მაგრამ EXPORT-სექციისთვის.
კითხულობს UEEC EXPORT-სექციას `COMPACT_GROUP`(`design=0`) ან `SIMPLE_FLAT`(`design=1`)
დიზაინით და გადაჰყავს **ნებისმიერ** ამ ორთაგან, ნებისმიერი მიმართულებით.

## საერთო შუალედური სტრუქტურა

ორივე დიზაინი (`COMPACT_GROUP`, `SIMPLE_FLAT`) ერთსა და იმავე ლოგიკურ
მონაცემს წარმოადგენს — `{section, offset, name}` entry-ების სია, სახელით
საძებნი — უბრალოდ სხვადასხვანაირად დაწყობილს (ორდონიანი group+binary-search
წინააღმდეგ ერთდონიანი binary-search). ამიტომ `decode_export()` ორივეს
შლის ერთნაირ `exp_entry{section,offset,name,namelen}` სიაში, `encode_*()`კი
ამ სიიდან აწყობს ნებისმიერ სამიზნე დიზაინს — ზუსტად `04_convert`-ის
decode→re-encode მიდგომის მიხედვით.

## რატომაა ORDINAL (design=2) ამ კონვერტერის scope-ის გარეთ

`COMPACT_GROUP`-სა და `SIMPLE_FLAT`-ს შორის კონვერტაცია **ინფორმაციის
დაკარგვის გარეშეა** (ორივეს აქვს ზუსტად იგივე `{section,offset,name}`
მონაცემი, სხვადასხვა wire-layout-ით). `ORDINAL` კი **პრინციპულად
განსხვავებულ keyspace-შია** — მას საერთოდ არა აქვს სახელის ველი, მხოლოდ
რიცხვითი `ordinal`. ასე რომ:

- **სახელით-დაფუძნებული → ORDINAL**: `ordinal`-ის მნიშვნელობა
  გამოგონებას მოითხოვდა (existing entry-ს არც ერთ ველში არაა
  შენახული) — ეს არ არის კონვერტაცია, არამედ ახალი მონაცემის დამატება.
- **ORDINAL → სახელით-დაფუძნებული**: სახელი საერთოდ არ არსებობს
  ფაილში — შეუქმნელია.

ეს იგივე პრინციპია, რითაც `04_convert`-მაც განზრახ გამოტოვა PE-style
მეოთხე RELOC დიზაინი (`reloc_v4.h`) — არა ტექნიკური სირთულის, არამედ
**სხვა მონაცემის მოდელის** გამო. `convert_export` ამიტომ ცხადად
ითხოვს `--design compact|flat` და ORDINAL-ზე ცდისას საგანგებო
შეტყობინებით წყდება (იხ. `decode_export()`-ში).

## გამოყენება

```
./convert_export input.ueec output.ueec --design compact
./convert_export input.ueec output.ueec --design flat
```

`loader_convert_export.c` ავტომატურად ცნობს input-ის დიზაინს
(`design`-ველიდან) და ორივესთვის სწორ binary-search ლოგიკას იყენებს
(`COMPACT_GROUP`-ისთვის ორდონიანს — length-ით, მერე სახელით; `SIMPLE_FLAT`
-ისთვის ერთდონიანს — პირდაპირ სახელით), რათა შედეგები პირდაპირ
შედარებადი იყოს.

## x86-64 round-trip ვალიდაცია

```
make run-all-directions
```

მიმდინარეობს: `COMPACT_GROUP`(ორიგინალი) → `SIMPLE_FLAT` → `COMPACT_GROUP`
(round-trip) → `SIMPLE_FLAT`(round-trip). **ორივე round-trip
ბაიტობრივად იდენტურია** ორიგინალთან (`cmp` — სხვაობა ვერ მოიძებნა).

## Multi-architecture ვალიდაცია — 10/10 architecture, ორივე დიზაინით

```
make all
```

**ყველა 10 architecture** (`arm32, arm64, riscv32, riscv64, powerpc32,
powerpc64, sparc32, sparc64, loongarch64, x86`) **+ x86-64** — **მწვანე**,
ორივე დიზაინით (`COMPACT_GROUP` და `SIMPLE_FLAT`), 4 value-lookup-ითა და
`answer()` function-call-ითურთ (`riscv32`-ისა და `sparc32`-ის ჩათვლით —
დამოწმებული მომხმარებლის მანქანაზე, სადაც ორივე toolchain-ია
დაყენებული; ამ სესიის კონტეინერს ორივე აკლდა, ix. ქვემოთ):

```
'counter' -> value = 42
'bar'     -> value = 11
'foo'     -> value = 22
'answer'  -> answer() = 42   (namdvili function-pointer call, arqiteqtura-specifiuri machine code)
'banana'  -> value = 33
'xyz', 'nonexistent' -> ver moidzebna (orive dizainSi swori)
```

### გზად ნაპოვნი და გასწორებული, ახალი კლასის ბაგები

ამ პროექტმა ორი ახალი, cross-architecture-სპეციფიური ბაგი გამოავლინა,
რომლებიც `08_export`-ში (ცალკეული, x86-64-ზე მხოლოდ დამოწმებული
COMPACT_GROUP/ORDINAL დემოები) დაუფიქსირებელი დარჩა, რადგან ის სესია
საერთოდ არ ცდილობდა სხვა architecture-ზე გაშვებას:

1. **`answer`-ის machine code ყოველ architecture-ზე თავისივეა.**
   `08_export`-ში ჩაშენებული `arch/answer_x86_64.bin` (`mov eax,42; ret`)
   x86-64-ისთვის ნამდვილი, სამუშაო x86-64 opcode-ებია — მაგრამ ამ
   ბაიტების ARM/PowerPC/RISC-V/SPARC/LoongArch instruction-decoder-ით
   გაშვება "Illegal instruction"/"Segmentation fault"-ს იწვევდა.
   ამოხსნილია: `answer.c` (`int answer(void){return 42;}`) ცალკე
   ნამდვილი cross-gcc-ით ითარგმნა თითოეული architecture-სთვის
   (`-O2 -fomit-frame-pointer -fno-asynchronous-unwind-tables
   -fno-stack-protector`), `objcopy --only-section=.text`-ით
   გამოღებული და `arch/answer_<arqiteqtura>.bin`-ში შენახული;
   `gen_export.c` ახლა პარამეტრიზებულია (`<code.bin> <UEEC_MACHINE_*>
   [be|le]`) და თითოეული architecture-სთვის სწორ node-ს აწყობს.

2. **ARM32 Thumb-interworking.** GCC-ის default `arm-linux-gnueabihf`
   target Thumb-2 code-ს აგენერირებდა (2-ბაიტიანი encoding-ებით), მაშინ
   როცა loader-ი პირდაპირ, plain function-pointer-ით უსვამდა call-ს
   (bit0=0 → CPU ARM-instruction-decoder-ში ცდილობდა Thumb bytes-ის
   decode-ს → Segmentation fault). გასწორებულია `-marm`-ით (იძულებით,
   32-ბიტიანი ARM encoding, არა Thumb) `answer_arm32.bin`-ის აგებისას.

3. **DATA content-ის endianness, ხელახლა.** `08_export`-ის დროს
   ნაპოვნი "DATA content-ის endianness" ბაგი (ix. `04_convert`/RELOC
   ისტორია) აქაც გამეორდა: `counter/bar/foo/banana`-ს int32
   მნიშვნელობები `gen_export.c`-ში ახლა target-architecture-ის
   ბუნებრივი byte-order-ით იწერება (`be`/`le` არგუმენტი) — BE
   architecture-ებზე (`powerpc32/64`, `sparc32/64`) დიდი-ბოლოთი,
   დანარჩენებზე მცირე-ბოლოთი, რადგან loader ამ ბაიტებს `memcpy`-ით
   პირდაპირ კითხულობს, structural endian-swap-ის (`rd16`/`rd32`) გარეშე.

4. **PowerPC64 ELFv1 function-descriptor.** `04_convert`/`07_pe`-დან
   ცნობილი ბაგი (`08_export`-ის `loader_export.c`-ში ჯერ არ იყო
   გასწორებული — `08_export` არასდროს გაშვებულა PowerPC64-ზე) აქაც
   გამოვლინდა და გასწორდა: `call_answer()` ცალკე ფუნქციაა,
   `#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)`
   შემთხვევაში `volatile unsigned long fdesc[3] = {addr,0,0}`
   descriptor-ტრიუკით.

## 10/10 architecture — დამოწმებულია მომხმარებლის მანქანაზე

ეს პროექტი თავდაპირველად 8/10-ზე შედგა ამ სესიის კონტეინერში (`sparc32`
-ს 32-ბიტიანი multilib header-ები აკლდა, `riscv32`-ს toolchain არ
ჰქონდა). მომხმარებელმა `make all` თავის მანქანაზე გაუშვა (სადაც ორივე
toolchain უკვე დაყენებული აქვს, `04_convert`-იდან) — **`sparc32`-მ
დაუყოვნებლივ გაიარა** (real machine code, ორივე დიზაინი, `answer()`
-ის ჩათვლით). `riscv32`-მ ვერ გაიარა მხოლოდ იმიტომ, რომ `arch/answer_
riscv32.bin` ამ zip-ში საერთოდ არ იყო ჩადებული (ეს სესია ვერ აშენებდა
მას toolchain-ის გარეშე).

**გასწორებულია და დადასტურებულია**: `arch/answer_<arqiteqtura>.bin`
-ები აღარაა წინასწარ-აშენებული, "ჩაბეტონებული" ბინარები, არამედ
`Makefile`-ს აქვს ცალკე rule თითოეულისთვის, რომელიც `arch/answer.c`
(`int answer(void){return 42;}`)-დან აშენებს cross-gcc-ით და
`objcopy --only-section=.text`-ით — **on demand**, თუ `.bin` ჯერ არ
არსებობს. მომხმარებელმა `make all` ხელახლა გაუშვა — `riscv32.bin`
ავტომატურად აშენდა `/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc`
-ით და **გაიარა კიდეც** (ორივე დიზაინი, `answer()`-ის ჩათვლით).
**საბოლოო შედეგი: 10/10 architecture, დადასტურებულია მომხმარებლის
საკუთარ მანქანაზე.**

## გზად ხელახლა გამოყენებული პრინციპები

1. **decode → საერთო შუალედური სია → encode** (`04_convert`-ის
   ზუსტი სქემა).
2. **round-trip ვალიდაცია** ბაიტობრივი `cmp`-ით, არა მხოლოდ
   ლოგიკური query-შედეგებით.
3. **format (16/32/64) ავტომატური არჩევა** საჭირო მაქსიმალური
   მნიშვნელობის მიხედვით (`format_for()`), `04_convert`-ის
   `encode_block`-ის ანალოგიით.
4. **PowerPC64 fdesc-ტრიუკი, DATA-endianness-ის target-მიხედვით
   არჩევა** — ორივე ხელახლა გამოყენებულია RELOC-ისეთივე ფორმით.
