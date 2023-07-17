-- File              : gv_sha256_v1_0.vhd
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

entity gv_sha256_v1_0 is
  generic (
    -- Parameters of Axi Slave Bus Interface S00_AXI
    C_S00_AXI_DATA_WIDTH : integer := 32;
    C_S00_AXI_ADDR_WIDTH : integer := 7
  );
  port (
    sha256_irq      : out std_logic;
    
    clocked_led     : out std_logic;
    activity_led    : out std_logic;

    -- Ports of Axi Slave Bus Interface S00_AXI
    s00_axi_aclk    : in std_logic;
    s00_axi_aresetn : in std_logic;
    s00_axi_awaddr  : in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
    s00_axi_awprot  : in std_logic_vector(2 downto 0);
    s00_axi_awvalid : in std_logic;
    s00_axi_awready : out std_logic;
    s00_axi_wdata   : in std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
    s00_axi_wstrb   : in std_logic_vector((C_S00_AXI_DATA_WIDTH/8)-1 downto 0);
    s00_axi_wvalid  : in std_logic;
    s00_axi_wready  : out std_logic;
    s00_axi_bresp   : out std_logic_vector(1 downto 0);
    s00_axi_bvalid  : out std_logic;
    s00_axi_bready  : in std_logic;
    s00_axi_araddr  : in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
    s00_axi_arprot  : in std_logic_vector(2 downto 0);
    s00_axi_arvalid : in std_logic;
    s00_axi_arready : out std_logic;
    s00_axi_rdata   : out std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
    s00_axi_rresp   : out std_logic_vector(1 downto 0);
    s00_axi_rvalid  : out std_logic;
    s00_axi_rready  : in std_logic
  );
end gv_sha256_v1_0;

architecture arch_imp of gv_sha256_v1_0 is

  -- component declaration
  component gv_sha256_v1_0_S00_AXI is
    generic (
      C_S_AXI_DATA_WIDTH : integer := 32;
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

      S_AXI_ACLK      : in std_logic;
      S_AXI_ARESETN   : in std_logic;
      S_AXI_AWADDR    : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_AWPROT    : in std_logic_vector(2 downto 0);
      S_AXI_AWVALID   : in std_logic;
      S_AXI_AWREADY   : out std_logic;
      S_AXI_WDATA     : in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_WSTRB     : in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
      S_AXI_WVALID    : in std_logic;
      S_AXI_WREADY    : out std_logic;
      S_AXI_BRESP     : out std_logic_vector(1 downto 0);
      S_AXI_BVALID    : out std_logic;
      S_AXI_BREADY    : in std_logic;
      S_AXI_ARADDR    : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
      S_AXI_ARPROT    : in std_logic_vector(2 downto 0);
      S_AXI_ARVALID   : in std_logic;
      S_AXI_ARREADY   : out std_logic;
      S_AXI_RDATA     : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
      S_AXI_RRESP     : out std_logic_vector(1 downto 0);
      S_AXI_RVALID    : out std_logic;
      S_AXI_RREADY    : in std_logic
    );
  end component gv_sha256_v1_0_S00_AXI;

  component gv_sha256 is
    port (
          -- clock and core enable
      clk_i : in std_logic := 'U';                                    -- system clock
      ce_i : in std_logic := 'U';                                     -- core clock enable
                                                                      -- input data
      di_i : in std_logic_vector (31 downto 0) := (others => 'U');    -- big endian input message words
      bytes_i : in std_logic_vector (1 downto 0) := (others => 'U');  -- valid bytes in input word
                                                                      -- start/end commands
      start_i : in std_logic := 'U';                                  -- reset the engine and start a new hash
      end_i : in std_logic := 'U';                                    -- marks end of last block data input
                                                                      -- handshake
      di_req_o : out std_logic;                                       -- requests data input for next word
      di_wr_i : in std_logic := 'U';                                  -- high for di_i valid, low for hold
      error_o : out std_logic;                                        -- signalizes error. output data is invalid
      do_valid_o : out std_logic;                                     -- when high, the output is valid
                                                                      -- 256bit output registers
      H0_o : out std_logic_vector (31 downto 0);
      H1_o : out std_logic_vector (31 downto 0);
      H2_o : out std_logic_vector (31 downto 0);
      H3_o : out std_logic_vector (31 downto 0);
      H4_o : out std_logic_vector (31 downto 0);
      H5_o : out std_logic_vector (31 downto 0);
      H6_o : out std_logic_vector (31 downto 0);
      H7_o : out std_logic_vector (31 downto 0)
    );
  end component;


  function and_reduce(arg: std_logic_vector) return std_logic is
    variable result: STD_LOGIC;
  begin
    result := '1';

    for i in arg'range loop
      result := result and arg(i);
    end loop;

    return result;
  end;

  type word_a is array (natural range<>) of std_logic_vector(31 downto 0);

  signal msg : word_a(0 to 15);
  signal hash : word_a(7 downto 0);

  signal cnt : std_logic_vector(3 downto 0);
  signal tc : std_logic;

  signal block_read : std_logic;

  signal wcur, wcur_big_endian : std_logic_vector(31 downto 0);

  signal csr_core_enable, csr_new_hash : std_logic;
  signal csr_last, last_eff : std_logic;
  signal csr_byte, byte_eff : std_logic_vector(1 downto 0);
  signal csr_last_word : std_logic_vector(3 downto 0);
  signal csr_wr, wr_eff : std_logic;
  signal csr_ienable : std_logic;
  signal csr_error : std_logic;
  signal csr_block_read : std_logic;

  signal gv_req : std_logic;
  signal gv_done : std_logic;

  signal irq_block_read, irq_done : std_logic;

begin

  -- Instantiation of Axi Bus Interface S00_AXI
  gv_sha256_v1_0_S00_AXI_inst : gv_sha256_v1_0_S00_AXI
  generic map (
    C_S_AXI_DATA_WIDTH => C_S00_AXI_DATA_WIDTH,
    C_S_AXI_ADDR_WIDTH => C_S00_AXI_ADDR_WIDTH
  )
  port map (
    csr_core_enable    => csr_core_enable,
    csr_new_hash       => csr_new_hash,
    csr_last           => csr_last,
    csr_vbyte          => csr_byte,
    csr_last_word      => csr_last_word,
    csr_wr             => csr_wr,
    csr_ienable        => csr_ienable,
    csr_block_read     => csr_block_read,
    csr_done           => gv_done,
    csr_error          => csr_error,

    tc                 => tc,

    hash_7             => hash(7),
    hash_6             => hash(6),
    hash_5             => hash(5),
    hash_4             => hash(4),
    hash_3             => hash(3),
    hash_2             => hash(2),
    hash_1             => hash(1),
    hash_0             => hash(0),

    word_0             => msg(0),
    word_1             => msg(1),
    word_2             => msg(2),
    word_3             => msg(3),
    word_4             => msg(4),
    word_5             => msg(5),
    word_6             => msg(6),
    word_7             => msg(7),
    word_8             => msg(8),
    word_9             => msg(9),
    word_10            => msg(10),
    word_11            => msg(11),
    word_12            => msg(12),
    word_13            => msg(13),
    word_14            => msg(14),
    word_15            => msg(15),

    S_AXI_ACLK         => s00_axi_aclk,
    S_AXI_ARESETN      => s00_axi_aresetn,
    S_AXI_AWADDR       => s00_axi_awaddr,
    S_AXI_AWPROT       => s00_axi_awprot,
    S_AXI_AWVALID      => s00_axi_awvalid,
    S_AXI_AWREADY      => s00_axi_awready,
    S_AXI_WDATA        => s00_axi_wdata,
    S_AXI_WSTRB        => s00_axi_wstrb,
    S_AXI_WVALID       => s00_axi_wvalid,
    S_AXI_WREADY       => s00_axi_wready,
    S_AXI_BRESP        => s00_axi_bresp,
    S_AXI_BVALID       => s00_axi_bvalid,
    S_AXI_BREADY       => s00_axi_bready,
    S_AXI_ARADDR       => s00_axi_araddr,
    S_AXI_ARPROT       => s00_axi_arprot,
    S_AXI_ARVALID      => s00_axi_arvalid,
    S_AXI_ARREADY      => s00_axi_arready,
    S_AXI_RDATA        => s00_axi_rdata,
    S_AXI_RRESP        => s00_axi_rresp,
    S_AXI_RVALID       => s00_axi_rvalid,
    S_AXI_RREADY       => s00_axi_rready
  );

  counter_p : process (s00_axi_aclk)
  begin
    if rising_edge(s00_axi_aclk) then
      if s00_axi_aresetn = '0' or tc = '1' then
        cnt <= (others => '0');
      elsif wr_eff = '1' then
        cnt <= std_logic_vector(unsigned(cnt) + 1);
      end if;
    end if;
  end process;

  -- register-programmed terminal count
  tc_p : tc <= and_reduce(cnt xnor csr_last_word); 

  -- word selector
  msg_word_p : wcur <= msg(to_integer(unsigned(cnt)));

  -- big endian conversion
  wcur_big_endian <= wcur(7 downto 0) & wcur(15 downto 8) & wcur(23 downto 16) & wcur(31 downto 24);

  -- byte masking
  last_eff <= csr_last and tc;
  byte_eff <= csr_byte when last_eff = '1' else
              "00";
  wr_eff <= csr_wr and gv_req;

  block_read <= tc and not csr_last and csr_wr;

    -- process to save block_read signal when it is raised
  block_read_p : process (s00_axi_aclk)
  begin
    if rising_edge(s00_axi_aclk) then
      if s00_axi_aresetn = '0' or (csr_wr = '1' and tc = '0') then
        csr_block_read <= '0';
      elsif block_read = '1' then
        csr_block_read <= '1';
      end if;
    end if;
  end process;

  gv_sha256_i : gv_sha256
  port map (
    clk_i      => s00_axi_aclk,
    ce_i       => csr_core_enable,
    di_i       => wcur_big_endian,
    bytes_i    => byte_eff,
    start_i    => csr_new_hash,
    end_i      => last_eff,

    di_req_o   => gv_req,
    di_wr_i    => wr_eff,
    error_o    => csr_error,
    do_valid_o => gv_done,

    H0_o       => hash(7),
    H1_o       => hash(6),
    H2_o       => hash(5),
    H3_o       => hash(4),
    H4_o       => hash(3),
    H5_o       => hash(2),
    H6_o       => hash(1),
    H7_o       => hash(0)
  );
  
  clocked_led   <= csr_core_enable;
  activity_led  <= block_read;
  
  -- level irq
  irq_done <= gv_done;
  
  -- edge irq
  irq_block_read <= block_read;
  
  -- irq_done is certainly the last edge
  sha256_irq <= (irq_done or irq_block_read) and csr_ienable;

end arch_imp;
