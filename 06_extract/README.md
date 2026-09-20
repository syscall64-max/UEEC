# 06_extract — node extraction

მრავალ-node-იანი UEEC ფაილიდან ერთი node-ის ამოღება, ახალი, **ერთ-node-იანი**
UEEC ფაილის შესაქმნელად.

## შერჩევის პარამეტრები (ნებისმიერი კომბინაცია)

```
--name N            # node-ის სახელი
--machine-type T     # 0=CPU, ...
--machine M          # 0=ARM32, 1=ARM64, ..., 9=X86, 10=X86_64
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
make all               # ყველა architecture (QEMU-ს ქვეშ)
make -k all            # თუ /opt/riscv32 არ გაქვთ
```

**20/20 (10 architecture × 2 load-mode) დამოწმებული** — ორივე extract-ისა
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
