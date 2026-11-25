# UEEC — AVR მიკროკონტროლერის ავტომატური არჩევა და ვირტუალური გაშვება

ერთი UEEC ფაილი შეიცავს სამ სხვადასხვა AVR მოდელზე აგებულ firmware-ს.
ჩამტვირთავი სიმულატორის მიკროკონტროლერის სიგნატურას კითხულობს, მეტამონაცემებით
ირჩევს შესაბამის ნოდს, მხოლოდ მის CODE-ს წერს ვირტუალურ Flash-ში და უშვებს.
შედეგი მოწმდება რეალური ემულირებული GPIO-ს გადასვლებით და ინახება VCD ტრასაში.

ეს ნამდვილი AVR მანქანური კოდის შესრულებაა **simavr**-ის CPU/პერიფერიის მოდელში.
LED-ის მდგომარეობები host-ის ტაიმერით ან წინასწარ დაწერილი ON/OFF სიით არ გენერირდება.

| მოდელი | GCC ISA პროფილი | Flash | SRAM | EEPROM | ვირტუალური CPU clock | LED სიგნალი |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| ATtiny85 | avr25 | 8 KiB | 512 B | 512 B | 8 MHz | PB0 |
| ATmega328P | avr5 | 32 KiB | 2 KiB | 1 KiB | 16 MHz | PB5 |
| ATmega2560 | avr6 | 256 KiB | 8 KiB | 4 KiB | 16 MHz | PB7 |

სამივე 8-ბიტიანი AVR-ია. GCC-ის `avr25`/`avr5`/`avr6` განსხვავებული სამიზნე
პროფილებია. ATmega2560-ს დაბრუნების მისამართისთვის 3-ბაიტიანი PC წარმოდგენა აქვს;
პირველ ორს — 2-ბაიტიანი. ცხრილის clock არის ამ მაგალითის კონფიგურაცია და არა
ქარხნული fuse-ების ან კონკრეტული დაფის ავტომატურად აღმოჩენილი სიხშირე.

## დაყენება და შემოწმება

Debian 13 / Ubuntu:

```sh
sudo apt install build-essential pkg-config python3 gcc-avr binutils-avr avr-libc simavr libsimavr-dev
cd 14_avr_metadata
make test-all
```

`make test-all` სამივე firmware-ს თავიდან აგებს და ყველა მოდელს ამოწმებს.
გამოტოვებული მოდელი PASS-ად არ ითვლება. `make test` იყენებს დართულ,
უკვე აგებულ `payloads/` ბინარებს; ამ შემთხვევაში AVR cross-კომპილატორი არ არის საჭირო,
მაგრამ host C კომპილატორი და `libsimavr-dev` საჭიროა.

## ხელით გაშვება

```sh
make all

# მხარდაჭერილი მოწყობილობების პროფილები
./build/ueec-avr --list-devices

# ვირტუალური მოწყობილობის შესაძლებლობები
./build/ueec-avr --mcu atmega328p --groups

# სამი ნოდის დათვალიერება და მოწყობილობასთან თავსებადობა
./build/ueec-avr --mcu atmega328p --list-nodes build/blink.ueec

# ავტომატური არჩევა, ვირტუალურ Flash-ში ჩაწერა და გაშვება
./build/ueec-avr --mcu attiny85 build/blink.ueec
./build/ueec-avr --mcu atmega328p build/blink.ueec
./build/ueec-avr --mcu atmega2560 build/blink.ueec

# ნოდის არჩევის ნახვა Flash-ის ჩაწერის გარეშე
./build/ueec-avr --mcu atmega2560 --no-run build/blink.ueec

# მხოლოდ არჩეული CODE-ის ბაიტების ამოღება
./build/ueec-avr --mcu atmega328p --extract build/atmega328p.bin --no-run build/blink.ueec

# რეალური GPIO ტრასის ჩაწერა
./build/ueec-avr --mcu atmega328p --vcd build/atmega328p.vcd build/blink.ueec
```

**`--mcu` აყენებს ვირტუალურ მოწყობილობას და ნოდის ინდექსს არ ნიშნავს.**
ნოდი თვითონ შეირჩევა ამ მოწყობილობის სიგნატურითა და შესაძლებლობებით.
ნაგულისხმევი ვირტუალური მოწყობილობაა ATmega328P.
simavr 1.6-მა მისი შიდა core-ის სახელად შეიძლება `atmega328` დაბეჭდოს;
ამ მაგალითში მოდელის არჩევას `1e950f` სიგნატურაც ამოწმებს.

გაშვებისას დაახლოებით ასეთი შედეგი მიიღება:

```text
SELECTED model=atmega328p node=1 CODE=2 METADATA=4 priority=100 mode=automatic
FLASH bytes=164 address=0 verified=yes metadata_written=no
GPIO edge=1 port=B pin=5 level=1 ... LED=ON
GPIO edge=2 port=B pin=5 level=0 ... LED=OFF
...
PASS: virtual Flash + AVR execution + 8 GPIO transitions; half_period_ms=100; timing_tolerance=1%
```

სიმულაცია რვა გადასვლის შემდეგ სრულდება. LED-ის ნახევარპერიოდია 100 ms:
შესაბამისად, ON/OFF სრული პერიოდია დაახლოებით 200 ms. დრო არის სიმულირებული
CPU-ს cycle-ებით გამოთვლილი დრო; სიმულაცია wall-clock-ზე სწრაფადაც შეიძლება დასრულდეს.
`--edges N` ცვლის გადასვლების რაოდენობას, ხოლო `--max-cycles N` ადგენს სატესტო ბიუჯეტს.

VCD-ის დათვალიერება შესაძლებელია, მაგალითად GTKWave-ში:

```sh
sudo apt install gtkwave
gtkwave build/atmega328p.vcd
```

სამივე უკვე შესრულებული მოდელის ლოგები და ტრასები დართულია `examples/traces/`-ში.
VCD შეიცავს GPIO-ს რეალურ დაკვირვებებს; ის დამოუკიდებელი MCU სიმულატორი არ არის.

## საერთო METADATA სექცია

გამოყენებულია იგივე shared design=1, რაც ეტაპი 13-ში:

```c
typedef struct UEEC_PACKED {
    uint16_t node_index;
    uint16_t type;
    uint16_t offset;
} ueec_md_node_element16;
```

არსებობს 32/64-ბიტიანი count/offset ვარიანტებიც; **node_index ყოველთვის 16-ბიტიანია**.
სამივე ნოდის METADATA მიბმულია ერთ საერთო სექციაზე, რომელიც ფაილის ბოლოშია.
ჩანაწერის ზომა მომდევნო offset-ით განისაზღვრება, ბოლოს კი END ჩანაწერია.

ყოველ ნოდს აქვს ოთხი ჩანაწერი:

| ტიპი | შიგთავსი |
| --- | --- |
| 0x200 | AVR_TARGET: სიგნატურა, core, PC/SP ზომები, Flash/SRAM/EEPROM, clock |
| 0x201 | AVR_REQUIRED: uint64 ISA მოთხოვნების ნიღაბი |
| 0x202 | AVR_BLINK: აპლიკაციის GPIO და ნახევარპერიოდი |
| 2 | PRIORITY: uint16 |

ეს მეტამონაცემების ნომრები ჯერ საცდელი შეთანხმებაა. GPIO ჩანაწერი აპლიკაციის
სატესტო კავშირია; MCU-ის შესაძლებლობების აღწერისგან ცალკე ინახება.

```c
typedef struct UEEC_PACKED {
    uint8_t signature[3];
    uint8_t gcc_arch;
    uint8_t pc_bytes, sp_bytes;
    uint16_t reserved;
    uint32_t flash_bytes, sram_bytes, eeprom_bytes, clock_hz;
} ueec_md_avr_target;

typedef struct UEEC_PACKED {
    uint64_t movw:1, lpmx:1, adiw_sbiw:1, mul:1, jmp_call:1,
             elpm:1, elpmx:1, eijmp_eicall:1, reserved:56;
} ueec_md_avr_caps;

typedef struct UEEC_PACKED {
    uint8_t port, pin;
    uint16_t half_period_ms;
} ueec_md_avr_blink;
```

ეს C ხედებია; ფაილში რიცხვები endian-ის გათვალისწინებით იწერება და იკითხება.
Bitfield-ების ABI განლაგება ფაილში პირდაპირ არ კოპირდება.
სამიზნის memory ზომები MCU-ის ტევადობებია, და არა ამ პროგრამის RAM-ის მინიმალური მოთხოვნა.
Flash-ის გვერდების ზომა მეტამონაცემებში არ შედის.

Firmware-ის სიგნატურა, memory ზომები, core და შესაძლებლობების ნიღაბი მიიღება
`avr-gcc -mmcu=...`-ის device header-ებიდან. REQUIRED ნიღაბი კონსერვატიულია:
იგი კომპილატორის სამიზნე პროფილის ჯგუფებს მოიცავს; ბინარის ზუსტი მინიმალური ISA-ს
ავტომატური ამოცნობა არ სრულდება. ეს არც ყველა AVR ბრძანების კატალოგია.

ჩამტვირთავის მეორე მხარეს simavr-ისგან იკითხება სიგნატურა, Flash/SRAM/EEPROM და
PC-ის ზომა; ISA ჯგუფები და SP/core კლასი ცნობილ პროფილებთანაა დაკავშირებული.
AVR-ს CPUID-ის მსგავსი გამოკითხვის მექანიზმი აქ არ მიეწერება.

## არჩევისა და გაშვების წესი

1. მოწმდება UEEC-ის სტრუქტურა და საერთო MD-ის ყველა ჩანაწერი.
2. ნოდი უნდა იყოს MPU/AVR8, standalone program, bare metal, little-endian CODE.
3. MCU-ის სიგნატურა და core/PC/SP/memory/clock პროფილი ზუსტად უნდა ემთხვეოდეს.
4. ყველა მოთხოვნილი ISA ბიტი ხელმისაწვდომი უნდა იყოს.
5. შესაბამის ნოდებს შორის ყველაზე მაღალი PRIORITY აირჩევა.
6. მხოლოდ მისი raw CODE იტვირთება Flash-ის მისამართზე 0 და ბაიტობრივად მოწმდება.
7. UEEC-ის buffer და მეტამონაცემების ცხრილი გაშვებამდე თავისუფლდება.
8. სრულდება ნამდვილი reset/startup/main კოდი; GPIO-ს ცვლილებები და პერიოდები მოწმდება.

მხოლოდ საერთო ISA საკმარისი არ არის: სხვადასხვა MCU-ს ვექტორები და პერიფერიის
რეგისტრები შეიძლება განსხვავდებოდეს. ამიტომ სხვა მოდელის firmware არ აირჩევა
ავტომატურ fallback-ად. შეცდომისას Flash-ის ჩაწერა არ სრულდება.

UEEC-ის არსებულ ინდექსებში გამოიყენება `machine_type=MPU (1)`, `machine=AVR8 (0)`,
`system_type=BARE_METAL (0)`. ძველი UEF-ის bare-metal ინდექსი ამაში არ ირევა.
ფაილშია **3 ნოდი, 3 CODE და 1 METADATA**, სულ 840 ბაიტი ამ აგების შედეგებით.

## საზღვრები და აგება

ეს ჩამტვირთავი host-ზე მუშაობს და სიმულატორის Flash-ის API-ს იყენებს.
ISP/STK500/Optiboot-ის ტრანზაქციები არ ემულირდება და ფიზიკურ მოწყობილობას არაფერი ეწერება.
ძველი UEF მაგალითების რეალურ მოწყობილობებზე მუშაობა ამ ახალი UEEC ვერსიის
ფიზიკურ ტესტად არ ითვლება. ასევე არ იწერება fuse-ები, EEPROM ან PCB-ის აღწერა.

```sh
make firmware
make generate
python3 generate.py --endian big --md-width 8 --output build/blink-be64.ueec
./build/ueec-avr --mcu attiny85 build/blink-be64.ueec
```

Firmware-ს აქვს საკუთარი reset vectors და AVR C startup; host-ის ELF პირდაპირ
არ ეშვება MCU-ზე. გენერატორი CODE-ის ბაიტებს SHA-256 მანიფესტით ამოწმებს.
ამ მაგალითის გენერატორის სექციების ფორმატია SH16, ამიტომ CODE-ის ერთი სექცია
65535 ბაიტზე დიდი ვერ იქნება. ATmega2560-ის სრული 256 KiB ტევადობის აღწერა
ამ მოცულობის firmware-ის ჩატვირთვის შემოწმებას არ ნიშნავს.

დამატებითი შემოწმება:

```sh
make test-sanitize
```

ptrace-ის გამო LeakSanitizer-ის მიუწვდომლობისას:
`ASAN_OPTIONS=detect_leaks=0 make test-sanitize`. ASan/UBSan ამ დროს აქტიურია.
ზუსტი შედეგებია [VALIDATION.md](VALIDATION.md)-ში.

## წყაროები და წარმომავლობა

- [simavr](https://github.com/buserror/simavr): AVR CPU/GPIO ემულატორი.
- [GCC AVR Options](https://gcc.gnu.org/onlinedocs/gcc/AVR-Options.html): სამიზნე core-ები და feature მაკროები.
- [ATtiny85](https://www.microchip.com/en-us/product/ATTINY85), [ATmega328P](https://www.microchip.com/en-us/product/ATmega328P), [ATmega2560](https://www.microchip.com/en-us/product/ATmega2560): მწარმოებლის მოწყობილობების აღწერა.

simavr არის გარე GPLv3 დამოკიდებულება; მისი source/library ბინარები არქივში არ არის.
UEEC-ის reference კოდის ლიცენზია შენარჩუნებულია `reference/LICENSE`-ში.
ამ მაგალითის კოდი, ტესტები და ტექსტი მომზადდა OpenAI-ის ასისტენტის მონაწილეობით.
