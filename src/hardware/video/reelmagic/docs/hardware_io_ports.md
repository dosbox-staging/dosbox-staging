



These notes are based on the Maxima board.

Base port is 0x260.

# I/O Operations

## Read 16-bit Word From ReelMagic Board

This is the foundation for all port I/O read operations from the board.

* Write `rmaddress` low byte to port base+0
* Write `rmaddress` high byte to port base+1
* Read `rmvalue` low byte from port base+2
* Read `rmvalue` high byte from port base+3

Watcom C Example:
```
static uint16_t
rmread(const uint16_t rmaddress) {
  uint16_t rv;
  outp(RM_BASE_ADDR + 0, rmaddress & 0xFF);
  outp(RM_BASE_ADDR + 1, rmaddress >> 8);
  rv  = (uint16_t)(inp(RM_BASE_ADDR + 2) & 0xFF);
  rv |= (uint16_t)(inp(RM_BASE_ADDR + 3) << 8);
  return rv;
}
```

## Write 16-bit Word To ReelMagic Board

This is the foundation for all port I/O write operations to the board.

* Write `rmaddress` low byte to port base+0
* Write `rmaddress` high byte to port base+1
* Write `rmvalue` low byte to port base+2
* Write `rmvalue` high byte to port base+3

Watcom C Example:
```
static void
rmwrite(const uint16_t rmaddress, const uint16_t rmvalue) {
  outp(RM_BASE_ADDR + 0, rmaddress & 0xFF);
  outp(RM_BASE_ADDR + 1, rmaddress >> 8);
  outp(RM_BASE_ADDR + 2, rmvalue & 0xFF);
  outp(RM_BASE_ADDR + 3, rmvalue >> 8);
}
```

## Write 16-bit Word to PL-450 DRAM

This procedure is used to directly write to the PL-450 chip's DRAM by FMPDRV.EXE when it is loading the
initial firmware.

* Write the target PL-450 DRAM word address to `rmaddress` 0x0001
* Write 0x0000 to `rmaddress` 0x0002 (IDK why, but FMPDRV.EXE always does this)
* Write the DRAM word value to `rmaddress` 0x0000

Note: The DRAM word address is half the value of the DRAM byte address. For example, if we wanted to
      write to DRAM byte address 8, we would use DRAM word address 4.

Watcom C Example:
```
static void
rmwrite450dram(const uint16_t dram_word_address, const uint16_t dram_word_value) {
  rmwrite(0x0001, dram_word_address);
  rmwrite(0x0002, 0x0000);
  rmwrite(0x0000, dram_word_value);
}
```




# Known ReelMagic Board Addresses

This is a listing of the known "rmaddress" values


## Address 0x0000 -- PL-450 DRAM Word Value

See `Write 16-bit Word to PL-450 DRAM` under the `I/O Operations` section.

## Address 0x0001 -- Set PL-450 DRAM Word Address

See `Write 16-bit Word to PL-450 DRAM` under the `I/O Operations` section.

## Address 0x0002 -- Used for PL-450 DRAM Write Word Operations

See `Write 16-bit Word to PL-450 DRAM` under the `I/O Operations` section.

## Addres 0x8010 -- Like PL-450 Control

After FMPDRV.EXE populates the DRAM and IMEM, it does this:
```
Write 8010 0001
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
... and so on...
```

## Address 0x8011 -- Set PL-450 Program Counter

FMPDRV.EXE writes to this address after it has written the firmware image to the DRAM and IMEM regions.
This sets the program counter in the PL-450's CPU to the given word value. The program counter value is
likely the count of 32-bit words from the beginning of the DRAM region. For example, if the desired
instruction to execute is located at byte address 0x1000 in the DRAM region, then the program counter
value would be 0x400.

## Address 0x801F -- Reset PL-450 IMEM Write Position (I think)

A write to this address with a value of zero is always performed by FMPDRV.EXE right before it starts
loading the "IMEM" region of the firmware. Likely writing to this sets the IMEM write position.

According to the PL-450 manual, the IMEM write position is controlled by the `CPU_iaddr` register on page 8-21.



## Address 0x8021 -- Append data to PL-450 IMEM

FMPDRV.EXE writes to this address when appending to the IMEM region of the PL-450 when performing the
initial firmware load. Each call to this automatically increments the position in IMEM that the word is
written to.

According to the PL-450 manual, when writing to IMEM, one must *ALWAYS* write two 16-bit words. The state
of the chip is undefined if this does not happen. See the `CPU_iaddr` register documented on page 8-21.

## Address 0x8040 -- Likely PL-450 CPU Control

Before firmware load, FMPDRV.EXE does the following with this address:
```
Read  8040 0010
Write 8040 0071
Read  8040 0010
Write 8040 0010
```

## Addres 0x8044 -- Like PL-450 Control

After FMPDRV.EXE populates the DRAM and IMEM, it does this:
```
Write 8010 0001
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
... and so on...
```


## Addres 0x8046 -- Like PL-450 Status of Some Kind

After FMPDRV.EXE populates the DRAM and IMEM, it does this:
```
Write 8010 0001
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
Read  8046 4400
Write 8044 0009
... and so on...
```



