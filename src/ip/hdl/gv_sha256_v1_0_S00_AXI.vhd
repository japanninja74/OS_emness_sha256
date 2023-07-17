-- File              : gv_sha256_v1_0_S00_AXI.vhd
--
-- Authors           : Fabio Scatozza      <s315216@studenti.polito.it>
--                     Isacco Delpero      <s314713@studenti.polito.it>
--                     Leonardo Cerruti    <s317664@studenti.polito.it>
--                     Claudio Capobianchi <s319187@studenti.polito.it>
--                     Saman Alipour       <s307980@studenti.polito.it>
--                     Abdul Rehman        <s315198@studenti.polito.it>
--                     Stephano Perera     <s313080@studenti.polito.it>
--
-- Date              : 01.07.2023
-- Last Modified Date: 05.07.2023
--
-- Copyright (c) 2023
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity gv_sha256_v1_0_S00_AXI is
  generic (
    -- Width of S_AXI data bus
    C_S_AXI_DATA_WIDTH : integer := 32;
    -- Width of S_AXI address bus
    C_S_AXI_ADDR_WIDTH : integer := 7
  );
  port (
    csr_core_enable : out std_logic;
    csr_new_hash    : out std_logic;
    csr_last        : out std_logic;
    csr_vbyte       : out std_logic_vector(1 downto 0);
    csr_last_word   : out std_logic_vector(3 downto 0);
    csr_wr          : out std_logic;
    csr_ienable     : out std_logic;
    csr_block_read  : in std_logic;
    csr_done        : in std_logic;
    csr_error       : in std_logic;

    tc              : in std_logic;

    hash_7          : in std_logic_vector(31 downto 0);
    hash_6          : in std_logic_vector(31 downto 0);
    hash_5          : in std_logic_vector(31 downto 0);
    hash_4          : in std_logic_vector(31 downto 0);
    hash_3          : in std_logic_vector(31 downto 0);
    hash_2          : in std_logic_vector(31 downto 0);
    hash_1          : in std_logic_vector(31 downto 0);
    hash_0          : in std_logic_vector(31 downto 0);

    word_0          : out std_logic_vector(31 downto 0);
    word_1          : out std_logic_vector(31 downto 0);
    word_2          : out std_logic_vector(31 downto 0);
    word_3          : out std_logic_vector(31 downto 0);
    word_4          : out std_logic_vector(31 downto 0);
    word_5          : out std_logic_vector(31 downto 0);
    word_6          : out std_logic_vector(31 downto 0);
    word_7          : out std_logic_vector(31 downto 0);
    word_8          : out std_logic_vector(31 downto 0);
    word_9          : out std_logic_vector(31 downto 0);
    word_10         : out std_logic_vector(31 downto 0);
    word_11         : out std_logic_vector(31 downto 0);
    word_12         : out std_logic_vector(31 downto 0);
    word_13         : out std_logic_vector(31 downto 0);
    word_14         : out std_logic_vector(31 downto 0);
    word_15         : out std_logic_vector(31 downto 0);

    -- Global Clock Signal
    S_AXI_ACLK      : in std_logic;
    -- Global Reset Signal. This Signal is Active LOW
    S_AXI_ARESETN   : in std_logic;
    -- Write address (issued by master, acceped by Slave)
    S_AXI_AWADDR    : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    -- Write channel Protection type. This signal indicates the
    -- privilege and security level of the transaction, and whether
    -- the transaction is a data access or an instruction access.
    S_AXI_AWPROT    : in std_logic_vector(2 downto 0);
    -- Write address valid. This signal indicates that the master signaling
    -- valid write address and control information.
    S_AXI_AWVALID   : in std_logic;
    -- Write address ready. This signal indicates that the slave is ready
    -- to accept an address and associated control signals.
    S_AXI_AWREADY   : out std_logic;
    -- Write data (issued by master, acceped by Slave) 
    S_AXI_WDATA     : in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    -- Write strobes. This signal indicates which byte lanes hold
    -- valid data. There is one write strobe bit for each eight
    -- bits of the write data bus.    
    S_AXI_WSTRB     : in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
    -- Write valid. This signal indicates that valid write
    -- data and strobes are available.
    S_AXI_WVALID    : in std_logic;
    -- Write ready. This signal indicates that the slave
    -- can accept the write data.
    S_AXI_WREADY    : out std_logic;
    -- Write response. This signal indicates the status
    -- of the write transaction.
    S_AXI_BRESP     : out std_logic_vector(1 downto 0);
    -- Write response valid. This signal indicates that the channel
    -- is signaling a valid write response.
    S_AXI_BVALID    : out std_logic;
    -- Response ready. This signal indicates that the master
    -- can accept a write response.
    S_AXI_BREADY    : in std_logic;
    -- Read address (issued by master, acceped by Slave)
    S_AXI_ARADDR    : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    -- Protection type. This signal indicates the privilege
    -- and security level of the transaction, and whether the
    -- transaction is a data access or an instruction access.
    S_AXI_ARPROT    : in std_logic_vector(2 downto 0);
    -- Read address valid. This signal indicates that the channel
    -- is signaling valid read address and control information.
    S_AXI_ARVALID   : in std_logic;
    -- Read address ready. This signal indicates that the slave is
    -- ready to accept an address and associated control signals.
    S_AXI_ARREADY   : out std_logic;
    -- Read data (issued by slave)
    S_AXI_RDATA     : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    -- Read response. This signal indicates the status of the
    -- read transfer.
    S_AXI_RRESP     : out std_logic_vector(1 downto 0);
    -- Read valid. This signal indicates that the channel is
    -- signaling the required read data.
    S_AXI_RVALID    : out std_logic;
    -- Read ready. This signal indicates that the master can
    -- accept the read data and response information.
    S_AXI_RREADY    : in std_logic
  );
end gv_sha256_v1_0_S00_AXI;

architecture arch_imp of gv_sha256_v1_0_S00_AXI is

  -- AXI4LITE signals
  signal axi_awaddr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
  signal axi_awready : std_logic;
  signal axi_wready  : std_logic;
  signal axi_bresp   : std_logic_vector(1 downto 0);
  signal axi_bvalid  : std_logic;
  signal axi_araddr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
  signal axi_arready : std_logic;
  signal axi_rdata   : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
  signal axi_rresp   : std_logic_vector(1 downto 0);
  signal axi_rvalid  : std_logic;

  -- Example-specific design signals
  -- local parameter for addressing 32 bit / 64 bit C_S_AXI_DATA_WIDTH
  -- ADDR_LSB is used for addressing 32/64 bit registers/memories
  -- ADDR_LSB = 2 for 32 bits (n downto 2)
  -- ADDR_LSB = 3 for 64 bits (n downto 3)
  constant ADDR_LSB          : integer := (C_S_AXI_DATA_WIDTH/32)+ 1;
  constant OPT_MEM_ADDR_BITS : integer := 4;

  ------------------------------------------------
  -- User logic register space
  ------------------------------------------------

  signal slv_last, slv_new_hash       : std_logic;
  signal slv_ce, slv_wr, slv_ienable  : std_logic;
  
  signal slv_vbyte              : std_logic_vector(1 downto 0);
  signal slv_last_word          : std_logic_vector(3 downto 0);

  signal slv_word_0, slv_word_1, slv_word_2    : std_logic_vector(31 downto 0);
  signal slv_word_3, slv_word_4, slv_word_5    : std_logic_vector(31 downto 0);
  signal slv_word_6, slv_word_7, slv_word_8    : std_logic_vector(31 downto 0);
  signal slv_word_9, slv_word_10, slv_word_11  : std_logic_vector(31 downto 0);
  signal slv_word_12, slv_word_13, slv_word_14 : std_logic_vector(31 downto 0);
  signal slv_word_15                           : std_logic_vector(31 downto 0);

  signal slv_reg_rden : std_logic;
  signal slv_reg_wren : std_logic;
  signal reg_data_out : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
  signal byte_index   : integer;
  signal aw_en        : std_logic;

begin
  -- I/O Connections assignments

  S_AXI_AWREADY	<= axi_awready;
  S_AXI_WREADY	<= axi_wready;
  S_AXI_BRESP	<= axi_bresp;
  S_AXI_BVALID	<= axi_bvalid;
  S_AXI_ARREADY	<= axi_arready;
  S_AXI_RDATA	<= axi_rdata;
  S_AXI_RRESP	<= axi_rresp;
  S_AXI_RVALID	<= axi_rvalid;

  -- Implement axi_awready generation
  -- axi_awready is asserted for one S_AXI_ACLK clock cycle when both
  -- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_awready is
  -- de-asserted when reset is low.

  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then
        axi_awready <= '0';
        aw_en <= '1';
      else
        if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1' and aw_en = '1') then
          -- slave is ready to accept write address when
          -- there is a valid write address and write data
          -- on the write address and data bus. This design 
          -- expects no outstanding transactions. 
          axi_awready <= '1';
          aw_en <= '0';
        elsif (S_AXI_BREADY = '1' and axi_bvalid = '1') then
          aw_en <= '1';
          axi_awready <= '0';
        else
          axi_awready <= '0';
        end if;
      end if;
    end if;
  end process;

  -- Implement axi_awaddr latching
  -- This process is used to latch the address when both 
  -- S_AXI_AWVALID and S_AXI_WVALID are valid. 

  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then
        axi_awaddr <= (others => '0');
      else
        if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1' and aw_en = '1') then
          -- Write Address latching
          axi_awaddr <= S_AXI_AWADDR;
        end if;
      end if;
    end if;                   
  end process; 

  -- Implement axi_wready generation
  -- axi_wready is asserted for one S_AXI_ACLK clock cycle when both
  -- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_wready is 
  -- de-asserted when reset is low. 

  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then
        axi_wready <= '0';
      else
        if (axi_wready = '0' and S_AXI_WVALID = '1' and S_AXI_AWVALID = '1' and aw_en = '1') then
          -- slave is ready to accept write data when 
          -- there is a valid write address and write data
          -- on the write address and data bus. This design 
          -- expects no outstanding transactions.           
          axi_wready <= '1';
        else
          axi_wready <= '0';
        end if;
      end if;
    end if;
  end process; 

  -- Implement memory mapped register select and write logic generation
  -- The write data is accepted and written to memory mapped registers when
  -- axi_awready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted. Write strobes are used to
  -- select byte enables of slave registers while writing.
  -- These registers are cleared when reset (active low) is applied.
  -- Slave register write enable is asserted when valid address and data are available
  -- and the slave is ready to accept the write address and write data.
  slv_reg_wren <= axi_wready and S_AXI_WVALID and axi_awready and S_AXI_AWVALID ;

  process (S_AXI_ACLK)
    variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0); 
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then

        slv_ce        <= '0';
        slv_vbyte     <= "00";
        slv_last_word <= (others => '0');
        slv_ienable   <= '0';
        slv_word_0    <= (others => '0');
        slv_word_1    <= (others => '0');
        slv_word_2    <= (others => '0');
        slv_word_3    <= (others => '0');
        slv_word_4    <= (others => '0');
        slv_word_5    <= (others => '0');
        slv_word_6    <= (others => '0');
        slv_word_7    <= (others => '0');
        slv_word_8    <= (others => '0');
        slv_word_9    <= (others => '0');
        slv_word_10   <= (others => '0');
        slv_word_11   <= (others => '0');
        slv_word_12   <= (others => '0');
        slv_word_13   <= (others => '0');
        slv_word_14   <= (others => '0');
        slv_word_15   <= (others => '0');

      else
        loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
        if (slv_reg_wren = '1') then
          case loc_addr is
            when b"00000" =>
              if ( S_AXI_WSTRB(0) = '1' ) then
                slv_ce        <= S_AXI_WDATA(0);
                slv_vbyte     <= S_AXI_WDATA(3 downto 2);
                slv_last_word <= S_AXI_WDATA(7 downto 4);
              end if;
              
              if (S_AXI_WSTRB(1) = '1' ) then
                slv_ienable   <= S_AXI_WDATA(10);
              end if;
            when b"00001" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 1
                  slv_word_0(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00010" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 2
                  slv_word_1(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00011" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 3
                  slv_word_2(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00100" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 4
                  slv_word_3(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00101" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 5
                  slv_word_4(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00110" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 6
                  slv_word_5(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"00111" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 7
                  slv_word_6(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01000" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 8
                  slv_word_7(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01001" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 9
                  slv_word_8(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01010" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 10
                  slv_word_9(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01011" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 11
                  slv_word_10(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01100" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 12
                  slv_word_11(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01101" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 13
                  slv_word_12(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01110" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 14
                  slv_word_13(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"01111" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 15
                  slv_word_14(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when b"10000" =>
              for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
                if ( S_AXI_WSTRB(byte_index) = '1' ) then
                  -- Respective byte enables are asserted as per write strobes                   
                  -- slave registor 16
                  slv_word_15(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
                end if;
              end loop;
            when others =>

              slv_ce <= slv_ce;
              slv_vbyte <= slv_vbyte;
              slv_last_word <= slv_last_word;
              slv_ienable <= slv_ienable;
              slv_word_0 <= slv_word_0;
              slv_word_1 <= slv_word_1;
              slv_word_2 <= slv_word_2;
              slv_word_3 <= slv_word_3;
              slv_word_4 <= slv_word_4;
              slv_word_5 <= slv_word_5;
              slv_word_6 <= slv_word_6;
              slv_word_7 <= slv_word_7;
              slv_word_8 <= slv_word_8;
              slv_word_9 <= slv_word_9;
              slv_word_10 <= slv_word_10;
              slv_word_11 <= slv_word_11;
              slv_word_12 <= slv_word_12;
              slv_word_13 <= slv_word_13;
              slv_word_14 <= slv_word_14;
              slv_word_15 <= slv_word_15;

          end case;
        end if;
      end if;
    end if;                   
  end process; 

  -- write to the outputs
  csr_core_enable <= slv_ce;
  csr_vbyte       <= slv_vbyte;
  csr_last_word   <= slv_last_word;
  csr_ienable     <= slv_ienable;
  word_0          <= slv_word_0;
  word_1          <= slv_word_1;
  word_2          <= slv_word_2;
  word_3          <= slv_word_3;
  word_4          <= slv_word_4;
  word_5          <= slv_word_5;
  word_6          <= slv_word_6;
  word_7          <= slv_word_7;
  word_8          <= slv_word_8;
  word_9          <= slv_word_9;
  word_10         <= slv_word_10;
  word_11         <= slv_word_11;
  word_12         <= slv_word_12;
  word_13         <= slv_word_13;
  word_14         <= slv_word_14;
  word_15         <= slv_word_15;

  -- Implement write response logic generation
  -- The write response and response valid signals are asserted by the slave 
  -- when axi_wready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted.  
  -- This marks the acceptance of address and indicates the status of 
  -- write transaction.

  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then
        axi_bvalid  <= '0';
        axi_bresp   <= "00"; --need to work more on the responses
      else
        if (axi_awready = '1' and S_AXI_AWVALID = '1' and axi_wready = '1' and S_AXI_WVALID = '1' and axi_bvalid = '0'  ) then
          axi_bvalid <= '1';
          axi_bresp  <= "00"; 
        elsif (S_AXI_BREADY = '1' and axi_bvalid = '1') then   --check if bready is asserted while bvalid is high)
          axi_bvalid <= '0';                                 -- (there is a possibility that bready is always asserted high)
        end if;
      end if;
    end if;                   
  end process; 

  -- Implement axi_arready generation
  -- axi_arready is asserted for one S_AXI_ACLK clock cycle when
  -- S_AXI_ARVALID is asserted. axi_awready is 
  -- de-asserted when reset (active low) is asserted. 
  -- The read address is also latched when S_AXI_ARVALID is 
  -- asserted. axi_araddr is reset to zero on reset assertion.

  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then 
      if S_AXI_ARESETN = '0' then
        axi_arready <= '0';
        axi_araddr  <= (others => '1');
      else
        if (axi_arready = '0' and S_AXI_ARVALID = '1') then
          -- indicates that the slave has acceped the valid read address
          axi_arready <= '1';
          -- Read Address latching 
          axi_araddr  <= S_AXI_ARADDR;           
        else
          axi_arready <= '0';
        end if;
      end if;
    end if;                   
  end process; 

  -- Implement axi_arvalid generation
  -- axi_rvalid is asserted for one S_AXI_ACLK clock cycle when both 
  -- S_AXI_ARVALID and axi_arready are asserted. The slave registers 
  -- data are available on the axi_rdata bus at this instance. The 
  -- assertion of axi_rvalid marks the validity of read data on the 
  -- bus and axi_rresp indicates the status of read transaction.axi_rvalid 
  -- is deasserted on reset (active low). axi_rresp and axi_rdata are 
  -- cleared to zero on reset (active low).  
  process (S_AXI_ACLK)
  begin
    if rising_edge(S_AXI_ACLK) then
      if S_AXI_ARESETN = '0' then
        axi_rvalid <= '0';
        axi_rresp  <= "00";
      else
        if (axi_arready = '1' and S_AXI_ARVALID = '1' and axi_rvalid = '0') then
          -- Valid read data is available at the read data bus
          axi_rvalid <= '1';
          axi_rresp  <= "00"; -- 'OKAY' response
        elsif (axi_rvalid = '1' and S_AXI_RREADY = '1') then
          -- Read data is accepted by the master
          axi_rvalid <= '0';
        end if;            
      end if;
    end if;
  end process;

  -- Implement memory mapped register select and read logic generation
  -- Slave register read enable is asserted when valid address is available
  -- and the slave is ready to accept the read address.
  slv_reg_rden <= axi_arready and S_AXI_ARVALID and (not axi_rvalid) ;

  process (
    slv_ce, slv_new_hash, slv_last, slv_vbyte, slv_last_word, 
    slv_wr, csr_error, csr_block_read, csr_done,
   
    hash_7, hash_6, hash_5, hash_4, hash_3, hash_2, hash_1, hash_0, slv_word_0, 
    
    slv_word_1, slv_word_2, slv_word_3, slv_word_4, slv_word_5, slv_word_6, 
    slv_word_7, slv_word_8, slv_word_9, slv_word_10, slv_word_11, slv_word_12, 
    slv_word_13, slv_word_14, slv_word_15, 

    axi_araddr, S_AXI_ARESETN, slv_reg_rden
  )
   
    variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0);
  
  begin
    -- Address decoding for reading registers
    loc_addr := axi_araddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
    case loc_addr is
      when b"00000" =>
        reg_data_out <= ( 
                          0  => slv_ce,
                          1  => slv_new_hash,
                          2  => slv_vbyte(0),
                          3  => slv_vbyte(1),
                          4  => slv_last_word(0),
                          5  => slv_last_word(1),
                          6  => slv_last_word(2),
                          7  => slv_last_word(3),
                          8  => slv_last,
                          9  => slv_wr,
                          10 => slv_ienable,

                          29 => csr_block_read,
                          30 => csr_done,
                          31 => csr_error,
                          others => '0'
                        );
      when b"00001" =>
        reg_data_out <= slv_word_0;
      when b"00010" =>
        reg_data_out <= slv_word_1;
      when b"00011" =>
        reg_data_out <= slv_word_2;
      when b"00100" =>
        reg_data_out <= slv_word_3;
      when b"00101" =>
        reg_data_out <= slv_word_4;
      when b"00110" =>
        reg_data_out <= slv_word_5;
      when b"00111" =>
        reg_data_out <= slv_word_6;
      when b"01000" =>
        reg_data_out <= slv_word_7;
      when b"01001" =>
        reg_data_out <= slv_word_8;
      when b"01010" =>
        reg_data_out <= slv_word_9;
      when b"01011" =>
        reg_data_out <= slv_word_10;
      when b"01100" =>
        reg_data_out <= slv_word_11;
      when b"01101" =>
        reg_data_out <= slv_word_12;
      when b"01110" =>
        reg_data_out <= slv_word_13;
      when b"01111" =>
        reg_data_out <= slv_word_14;
      when b"10000" =>
        reg_data_out <= slv_word_15;
      when b"10001" =>
        reg_data_out <= hash_7;
      when b"10010" =>
        reg_data_out <= hash_6;
      when b"10011" =>
        reg_data_out <= hash_5;
      when b"10100" =>
        reg_data_out <= hash_4;
      when b"10101" =>
        reg_data_out <= hash_3;
      when b"10110" =>
        reg_data_out <= hash_2;
      when b"10111" =>
        reg_data_out <= hash_1;
      when b"11000" =>
        reg_data_out <= hash_0;
      when others =>
        reg_data_out  <= (others => '0');
    end case;
  end process; 

  -- Output register or memory read data
  process( S_AXI_ACLK ) is
  begin
    if (rising_edge (S_AXI_ACLK)) then
      if ( S_AXI_ARESETN = '0' ) then
        axi_rdata  <= (others => '0');
      else
        if (slv_reg_rden = '1') then
          -- When there is a valid read address (S_AXI_ARVALID) with 
          -- acceptance of read address by the slave (axi_arready), 
          -- output the read dada 
          -- Read address mux
          axi_rdata <= reg_data_out;     -- register read data
        end if;   
      end if;
    end if;
  end process;


  -- Add user logic here
  ff_last : process (S_AXI_ACLK)
    variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0);
  begin
    if rising_edge(S_AXI_ACLK) then
      if S_AXI_ARESETN = '0' or slv_new_hash = '1' then
        slv_last <= '0';
      else
        loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
        if slv_reg_wren = '1' and loc_addr = b"00000" then
          if S_AXI_WSTRB(1) = '1' then --second least significant byte
            slv_last <= S_AXI_WDATA(8); -- last is bit 8
          end if;
        end if;
      end if;
    end if;
  end process;

  csr_last <= slv_last;

  ff_new_hash : process (S_AXI_ACLK)
    variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0);
  begin
    if rising_edge(S_AXI_ACLK) then
      if S_AXI_ARESETN = '0' or slv_new_hash = '1' then
        slv_new_hash <= '0';
      else
        loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
        if slv_reg_wren = '1' and loc_addr = b"00000" then
          if S_AXI_WSTRB(0) = '1' then -- least significant byte
            slv_new_hash <= S_AXI_WDATA(1); --new hash is bit 1
          end if;
        end if;
      end if;
    end if;
  end process;

  csr_new_hash <= slv_new_hash;

  ff_wr : process (S_AXI_ACLK)
    variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS downto 0);
  begin
    if rising_edge(S_AXI_ACLK) then
      if S_AXI_ARESETN = '0' or (tc = '1' and slv_wr = '1') then
        slv_wr <= '0';
      else
        loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS downto ADDR_LSB);
        if slv_reg_wren = '1' and loc_addr = b"00000" then
          if S_AXI_WSTRB(1) = '1' then --second least significant byte
            slv_wr <= S_AXI_WDATA(9); -- wr is bit 9
          end if;
        end if;
      end if;
    end if;
  end process;

  csr_wr <= slv_wr;

end arch_imp;
