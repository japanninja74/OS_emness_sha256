-----------------------------------------------------------------------------------------------------------------------
-- Author:          Jonny Doin, jdoin@opencores.org, jonnydoin@gmail.com
-- 
-- Create Date:     09:56:30 07/06/2011  
-- Module Name:     sha256_Ki_rom
-- Project Name:    sha256 processor
-- Target Devices:  Spartan-6
-- Tool versions:   ISE 14.7
-- Description: 
--
--      Initial values for the hash result registers.
--      This module is modelled as a fixed value function.
--      It can be implemented as a local constant fixed value.
--
------------------------------ COPYRIGHT NOTICE -----------------------------------------------------------------------
--                                                                   
--                                                                   
--      Author(s):      Jonny Doin, jonnydoin@gridvortex.com, jonnydoin@gmail.com
--                                                                   
--      Copyright (C) 2016 GridVortex, All Rights Reserved
--      --------------------------------------------------
--                                                                   
------------------------------ REVISION HISTORY -----------------------------------------------------------------------
--
-- 2016/05/22   v0.01.0010  [JD]    started development. design of blocks and port interfaces.
-- 2016/06/05   v0.01.0090  [JD]    all modules integrated. testbench for basic test vectors verification.
-- 2016/06/05   v0.01.0095  [JD]    verification failed. misalignment of words in the datapath. 
-- 2016/06/06   v0.01.0100  [JD]    first simulation verification against NIST-FIPS-180-4 test vectors passed.
--
-----------------------------------------------------------------------------------------------------------------------
--  TODO
--  ====
--
--
-----------------------------------------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity sha256_ki_rom is
    port (  
        K0_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K1_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K2_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K3_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K4_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K5_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K6_o : out std_logic_vector (31 downto 0) := (others => 'X');
        K7_o : out std_logic_vector (31 downto 0) := (others => 'X')
    );                      
end sha256_ki_rom;

architecture behavioral of sha256_ki_rom is
begin
    --=============================================================================================
    -- CONSTANTS FOR Ki VALUES
    --=============================================================================================
    K0_o_proc:      K0_o <= x"6A09E667";
    K1_o_proc:      K1_o <= x"BB67AE85";
    K2_o_proc:      K2_o <= x"3C6EF372";
    K3_o_proc:      K3_o <= x"A54FF53A";
    K4_o_proc:      K4_o <= x"510E527F";
    K5_o_proc:      K5_o <= x"9B05688C";
    K6_o_proc:      K6_o <= x"1F83D9AB";
    K7_o_proc:      K7_o <= x"5BE0CD19";

end behavioral;

