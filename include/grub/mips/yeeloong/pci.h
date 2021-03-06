/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	GRUB_MACHINE_PCI_H
#define	GRUB_MACHINE_PCI_H	1

#include <grub/types.h>
#include <grub/cpu/io.h>

#define GRUB_PCI_NUM_BUS        1
#define GRUB_PCI_NUM_DEVICES    16

#define GRUB_MACHINE_PCI_IO_BASE          0xbfd00000
#define GRUB_MACHINE_PCI_CONFSPACE        0xbfe80000
#define GRUB_MACHINE_PCI_CONF_CTRL_REG    (*(volatile grub_uint32_t *) 0xbfe00118)
#define GRUB_MACHINE_PCI_IO_CTRL_REG      (*(volatile grub_uint32_t *) 0xbfe00110)
#define GRUB_MACHINE_PCI_WIN_MASK_SIZE    6
#define GRUB_MACHINE_PCI_WIN_MASK         ((1 << GRUB_MACHINE_PCI_WIN_MASK_SIZE) - 1)

/* We have 3 PCI windows.  */
#define GRUB_MACHINE_PCI_NUM_WIN          3
/* Each window is 64MiB.  */
#define GRUB_MACHINE_PCI_WIN_SHIFT        26
#define GRUB_MACHINE_PCI_WIN_OFFSET_MASK  ((1 << GRUB_MACHINE_PCI_WIN_SHIFT) - 1)

#define GRUB_MACHINE_PCI_WIN_SIZE         0x04000000
/* Graphical acceleration takes 1 MiB away.  */
#define GRUB_MACHINE_PCI_WIN1_SIZE        0x03f00000

#define GRUB_MACHINE_PCI_WIN1_ADDR        0xb0000000
#define GRUB_MACHINE_PCI_WIN2_ADDR        0xb4000000
#define GRUB_MACHINE_PCI_WIN3_ADDR        0xb8000000

static inline grub_uint32_t
grub_pci_read (grub_pci_address_t addr)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  return *(volatile grub_uint32_t *) (GRUB_MACHINE_PCI_CONFSPACE
				      | (addr & 0x03ff));
}

static inline grub_uint16_t
grub_pci_read_word (grub_pci_address_t addr)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  return *(volatile grub_uint16_t *) (GRUB_MACHINE_PCI_CONFSPACE
				      | (addr & 0x03ff));
}

static inline grub_uint8_t
grub_pci_read_byte (grub_pci_address_t addr)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  return *(volatile grub_uint8_t *) (GRUB_MACHINE_PCI_CONFSPACE
				     | (addr & 0x03ff));
}

static inline void
grub_pci_write (grub_pci_address_t addr, grub_uint32_t data)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  *(volatile grub_uint32_t *) (GRUB_MACHINE_PCI_CONFSPACE
			       | (addr & 0x03ff)) = data;
}

static inline void
grub_pci_write_word (grub_pci_address_t addr, grub_uint16_t data)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  *(volatile grub_uint16_t *) (GRUB_MACHINE_PCI_CONFSPACE
			       | (addr & 0x03ff)) = data;
}

static inline void
grub_pci_write_byte (grub_pci_address_t addr, grub_uint8_t data)
{
  GRUB_MACHINE_PCI_CONF_CTRL_REG = 1 << ((addr >> 11) & 0xf);
  *(volatile grub_uint8_t *) (GRUB_MACHINE_PCI_CONFSPACE
			      | (addr & 0x03ff)) = data;
}

volatile void *
grub_pci_device_map_range (grub_pci_device_t dev __attribute__ ((unused)),
			   grub_addr_t base, grub_size_t size);
void
grub_pci_device_unmap_range (grub_pci_device_t dev __attribute__ ((unused)),
			     volatile void *mem,
			     grub_size_t size __attribute__ ((unused)));

#endif /* GRUB_MACHINE_PCI_H */
