library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity uart_rx is
    generic (
        CLKS_PER_BIT : integer := 5208  -- 50MHz / 9600 baud = 5208 ;  434 = 115200
    );
    port (
        i_Clk       : in  std_logic;
        i_RX_Serial : in  std_logic;
        o_RX_DV     : out std_logic; -- Data Valid (Pulso de 1 clock quando chega dado)
        o_RX_Byte   : out std_logic_vector(7 downto 0)
    );
end entity;

architecture rtl of uart_rx is
    type t_SM_Main is (s_Idle, s_RX_Start_Bit, s_RX_Data_Bits, s_RX_Stop_Bit, s_Cleanup);
    signal r_SM_Main : t_SM_Main := s_Idle;
    
    signal r_Clk_Count : integer range 0 to CLKS_PER_BIT-1 := 0;
    signal r_Bit_Index : integer range 0 to 7 := 0; -- 8 Bits Total
    signal r_RX_Byte   : std_logic_vector(7 downto 0) := (others => '0');
    signal r_RX_DV     : std_logic := '0';
    
begin
    process (i_Clk)
    begin
        if rising_edge(i_Clk) then
            case r_SM_Main is
                when s_Idle =>
                    r_RX_DV <= '0';
                    r_Clk_Count <= 0;
                    r_Bit_Index <= 0;
                    if i_RX_Serial = '0' then       -- Detectou Start Bit (Baixo)
                        r_SM_Main <= s_RX_Start_Bit;
                    else
                        r_SM_Main <= s_Idle;
                    end if;

                -- Verifica se o Start Bit é válido (meio do bit)
                when s_RX_Start_Bit =>
                    if r_Clk_Count = (CLKS_PER_BIT-1)/2 then
                        if i_RX_Serial = '0' then
                            r_Clk_Count <= 0;
                            r_SM_Main   <= s_RX_Data_Bits;
                        else
                            r_SM_Main   <= s_Idle;
                        end if;
                    else
                        r_Clk_Count <= r_Clk_Count + 1;
                        r_SM_Main   <= s_RX_Start_Bit;
                    end if;

                -- Leitura dos 8 Bits de Dados
                when s_RX_Data_Bits =>
                    if r_Clk_Count < CLKS_PER_BIT-1 then
                        r_Clk_Count <= r_Clk_Count + 1;
                        r_SM_Main   <= s_RX_Data_Bits;
                    else
                        r_Clk_Count <= 0;
                        r_RX_Byte(r_Bit_Index) <= i_RX_Serial;
                        
                        if r_Bit_Index < 7 then
                            r_Bit_Index <= r_Bit_Index + 1;
                            r_SM_Main   <= s_RX_Data_Bits;
                        else
                            r_Bit_Index <= 0;
                            r_SM_Main   <= s_RX_Stop_Bit;
                        end if;
                    end if;

                -- Stop Bit
                when s_RX_Stop_Bit =>
                    if r_Clk_Count < CLKS_PER_BIT-1 then
                        r_Clk_Count <= r_Clk_Count + 1;
                        r_SM_Main   <= s_RX_Stop_Bit;
                    else
                        r_RX_DV     <= '1'; -- Dado pronto!
                        r_Clk_Count <= 0;
                        r_SM_Main   <= s_Cleanup;
                    end if;

                when s_Cleanup =>
                    r_SM_Main <= s_Idle;
                    r_RX_DV   <= '0';
                    
                when others =>
                    r_SM_Main <= s_Idle;
            end case;
        end if;
    end process;
    
    o_RX_DV   <= r_RX_DV;
    o_RX_Byte <= r_RX_Byte;
    
end architecture;