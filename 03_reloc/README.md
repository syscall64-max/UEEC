# 03_reloc — SECTION_FLAT RELOC + ახალი _GM core

ერთი UEEC container (`_GM` header-ფორმატი) — 1 HEADER + 1 DATA(გაზიარებული,
`"Hello world!\n"`) + 12 × (CODE+RELOCATION), `SECTION_FLAT` RELOC-დიზაინით
(`reloc_v1.h` — `couple`+`info`, page/block-ის გარეშე).

## ფაილები

- `03_reloc.asm` — თვითკმარი NASM container (279 ხაზი-მსგავსი, `db`/`dw`/`dd` ბაიტები)
- `gen.c` — generator, რომელმაც `.asm` შექმნა (dev-ხელსაწყო, საბოლოო deliverable არაა)
- `loader.c` — loader, ორი load-mode-ით (`--L=monolithic`, `--L=scattered`)
- `ueec.h` — ახალი `_GM` core header (`George Modebadze`-ის სპეციფიკაცია)
- `reloc.h` — `SECTION_FLAT` RELOC structures (`reloc_v1.h`)

## გაშვება

```
make run-mono         # x86-64, monolithic
make run-scattered      # x86-64, scattered
make all               # ყველა architecture, ორივე რეჟიმში (QEMU-ს ქვეშ)
make -k all            # თუ /opt/riscv32 არ გაქვთ
```

**ისტორიული ტესტები: 20/20 (10 architecture × 2 load-mode)** — PowerPC32/64, SPARC32/64
(big-endian) ჩათვლით.

## RELOC-მექანიზმი (SECTION_FLAT)

```
ueec_reloc(format+design) -> ueec_reloc_couple16(target+source+format+count)
  -> count x ueec_reloc_info(offset:12+type:4)
```

`target`/`source` — node-ის local (non-HEADER) სექცია-ინდექსები (`local 0`=DATA,
`local 1`=CODE). `addend`(CODE-ში ჩაწერილი, ყოველთვის `0`, ვინაიდან ყველა node
ერთსა და იმავე DATA-offset `0`-ს იყენებს — ერთადერთი, საერთო სტრიქონია).

## ⚠️ ნაპოვნი და გასწორებული ბაგი

**`monolithic` mode-ის DATA→CODE alignment.** DATA (`13` ბაიტი) და CODE
თავდაპირველად უშუალოდ ერთმანეთის მიყოლებით იწერებოდა monolithic buffer-ში —
CODE არაგასწორებულ (`13 % 4 = 1`) მისამართზე მოხვდებოდა, რაც ARM32/64-სა და
PowerPC32/64-ზე instruction-fetch-ს ამტვრევდა (segfault/bus-error/illegal
instruction). გასწორდა `16`-ბაიტიანი alignment-ის დამატებით monolithic-ის
თითოეულ სექციას შორის.

## ცნობილი შეზღუდვები

- `riscv32` საჭიროებს `/opt/riscv32` toolchain-ს.
- `loader.c`-ის `mprotect(..., PROT_READ|PROT_EXEC|PROT_WRITE)` (monolithic)
  არის მარტივი test-harness-ის გამარტივება (W+X) — production-ში ცალკე
  write-ფაზა და exec-ფაზა უნდა გაიყოს.

## Loader update: temporary RELOCATION mappings

The loader now keeps RELOCATION sections out of the resident runtime image.

- `monolithic`: resident DATA + CODE are allocated inside one anonymous mapping, with page-separated ranges so final permissions can be `DATA=R`, `CODE=RX`.
- `scattered`: each resident section has its own anonymous mapping and is filled with `preadv()`.
- RELOCATION: each selected relocation section is loaded into its own temporary anonymous mapping, applied, and immediately released with `munmap()`.
- The UEEC file descriptor is closed after relocation, before control is transferred to CODE.
- The loader no longer reads the complete UEEC file into a malloc buffer.

Runtime after relocation therefore contains only the resident sections required by the selected node; the RELOCATION mapping no longer exists.


## IBM Z / S390X (12-ე არქიტექტურა)

- machine ID = `11`, კოდის ბაიტების მიმდევრობა Big-Endian, UEEC ფაილის მეტამონაცემები Little-Endian.
- `arch/s390x.s` — Linux s390x raw code, რომელიც `write(1, "Hello world!\n", 13)` და `exit(0)` სისტემურ გამოძახებებს იყენებს.
- `arch/s390x.bin` — მისგან მიღებული 40-ბაიტიანი CODE. ბოლო 8 ბაიტი (offset `32`) არის `data_address` ლიტერალი.
- შესაბამისი RELOCATION სექცია იყენებს `SECTION_FLAT`, `target=CODE`, `source=shared DATA`, `offset=32`, `type=ABS64`.
- `gen.c` აგებს 12-ნოდიან NASM წყაროს (`make regen`), დანარჩენი 11 არქიტექტურის კოდის payload უცვლელადაა გადატანილი საწყისი `03_reloc.asm`-იდან.
- სულ: 1 HEADER + 1 shared DATA + 12 CODE + 12 RELOCATION = **26 სექცია**; კონტეინერი **1116 ბაიტი**.

```bash
make clean
make check-s390x
make all
# წყაროებისა და .bin ბაიტების შესაბამისობა:
make verify-s390x-bytes
# საჭიროებისამებრ ხელახლა გენერაცია:
make regen
```

S390X ტესტი ორჯერ უშვებს ნამდვილ UEEC loader-ს `qemu-s390x`-ში:
`--L=monolithic` და `--L=scattered`. ორივე შემთხვევაში მოსალოდნელია
`RELOC: local[1]+0x20 <- local[0] + addend (ABS64)` და `Hello world!`.

**სიფრთხილე:** სრული 12-არქიტექტურული `make all` უნდა გადამოწმდეს
მომხმარებლის Ubuntu/QEMU გარემოში. აქ ლოკალურად შესრულებულია x86-64-ის
ორივე რეჟიმი. ეს არ ნიშნავს IBM z17-ის კონკრეტული ISA გაფართოებების ტესტირებას.
