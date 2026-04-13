library ieee;
use ieee.std_logic_1164.all;

entity Hardware is
    port (
        CLOCK_50 : in  std_logic;
        CLOCK_27 : in  std_logic;
        KEY      : in  std_logic_vector(3 downto 0);   -- Botoes da DE2
        SW       : in  std_logic_vector(16 downto 0);  -- Chaves da DE2
		  
        UART_RXD : in  std_logic;
        UART_TXD : out std_logic;

        PS2_CLK  : inout std_logic;
        PS2_DAT  : inout std_logic;

        -- Pinos da Matriz de LED (HUB75)
        R1, G1, B1 : out std_logic;
        R2, G2, B2 : out std_logic;
        A, B, C, D, E : out std_logic;
        CLK_OUT : out std_logic;
        LAT     : out std_logic;
        OE      : out std_logic;

        DRAM_ADDR  : out   std_logic_vector(11 downto 0);                    -- addr
        DRAM_BA    : out   std_logic_vector(1 downto 0);                     -- ba
        DRAM_CAS_N : out   std_logic;                                        -- cas_n
        DRAM_CKE   : out   std_logic;                                        -- cke
        DRAM_CS_N  : out   std_logic;                                        -- cs_n
        DRAM_DQ    : inout std_logic_vector(15 downto 0) := (others => 'X'); -- dq
        DRAM_DQM   : out   std_logic_vector(1 downto 0);                     -- dqm
        DRAM_RAS_N : out   std_logic;                                        -- ras_n
        DRAM_WE_N  : out   std_logic;                                        -- we_n
        DRAM_CLK   : out   std_logic;
		  
		  SD_DAT0       : in    std_logic  	:= 'X'; 							      	-- MISO
		  SD_CMD        : out   std_logic;                								-- MOSI
		  SD_CLK        : out   std_logic;                								-- SCLK
		  SD_DAT3       : out   std_logic;               								   -- SS_n
		  
	     -- Flash
        FL_ADDR     : out   std_logic_vector(21 downto 0);
        FL_DQ       : inout std_logic_vector(7 downto 0);
        FL_CE_N     : out   std_logic;
        FL_OE_N     : out   std_logic;
        FL_WE_N     : out   std_logic;
        FL_RST_N    : out   std_logic;
        FL_BYTE_N   : out   std_logic;
        FL_WP_N     : out   std_logic;
        FL_DQ15_AM1 : out   std_logic;
		  
        -- Pinos da Matriz de Audio
        I2C_SDAT : inout std_logic;
        I2C_SCLK : out   std_logic
    );
end entity;

architecture rtl of Hardware is

    -- Sinais internos para o barramento de dados flash
    signal flash_data_out   : std_logic_vector(7 downto 0);
    signal flash_data_in    : std_logic_vector(7 downto 0);
    signal flash_data_outen : std_logic;

    -- Sinais para request/grant (tristate bus arbitration)
    signal flash_request    : std_logic;
    signal flash_grant      : std_logic;
    signal flash_read_n     : std_logic;


    -- Declaracao do componente gerado pelo Qsys
    component niosII_ps2 is
        port (
            clk_clk       : in    std_logic                     := 'X';              -- clk
            botoes_export : in    std_logic_vector(3 downto 0)  := (others => 'X'); -- export

            leds_G1       : out   std_logic;                                        -- G1
            leds_B1       : out   std_logic;                                        -- B1
            leds_R2       : out   std_logic;                                        -- R2
            leds_G2       : out   std_logic;                                        -- G2
            leds_B2       : out   std_logic;                                        -- B2
            leds_A        : out   std_logic;                                        -- A
            leds_B        : out   std_logic;                                        -- B
            leds_C        : out   std_logic;                                        -- C
            leds_D        : out   std_logic;                                        -- D
            leds_E        : out   std_logic;                                        -- E
            leds_CLK      : out   std_logic;                                        -- CLK
            leds_LAT      : out   std_logic;                                        -- LAT
            leds_OE       : out   std_logic;                                        -- OE
            leds_R1       : out   std_logic;                                        -- R1
            reset_reset_n : in    std_logic                     := 'X';              -- reset_n
            ps2_CLK       : inout std_logic                     := 'X';              -- CLK
            ps2_DAT       : inout std_logic                     := 'X';              -- DAT
            sdram_addr    : out   std_logic_vector(11 downto 0);                    -- addr
            sdram_ba      : out   std_logic_vector(1 downto 0);                     -- ba
            sdram_cas_n   : out   std_logic;                                        -- cas_n
            sdram_cke     : out   std_logic;                                        -- cke
            sdram_cs_n    : out   std_logic;                                        -- cs_n
            sdram_dq      : inout std_logic_vector(15 downto 0) := (others => 'X'); -- dq
            sdram_dqm     : out   std_logic_vector(1 downto 0);                     -- dqm
            sdram_ras_n   : out   std_logic;                                        -- ras_n
            sdram_we_n    : out   std_logic;                                        -- we_n
				
				
            i2c_clk_external_connection_export : out   std_logic;                   -- export
            i2c_sda_external_connection_export : inout std_logic := 'X';            -- export
				
				sd_spi_0_external_MISO             : in    std_logic := 'X';       -- MISO
				sd_spi_0_external_MOSI             : out   std_logic;                -- MOSI
				sd_spi_0_external_SCLK             : out   std_logic;                -- SCLK
				sd_spi_0_external_SS_n             : out   std_logic;                -- SS_n
				
				generic_tristate_controller_0_tcm_write_n_out      : out   std_logic;                                        -- write_n_out
            generic_tristate_controller_0_tcm_read_n_out       : out   std_logic;                                        -- read_n_out
            generic_tristate_controller_0_tcm_chipselect_n_out : out   std_logic;                                        -- chipselect_n_out
            generic_tristate_controller_0_tcm_request          : out   std_logic;                                        -- request
            generic_tristate_controller_0_tcm_grant            : in    std_logic                     := 'X';             -- grant
            generic_tristate_controller_0_tcm_address_out      : out   std_logic_vector(21 downto 0);                    -- address_out
            generic_tristate_controller_0_tcm_data_out         : out   std_logic_vector(7 downto 0);                     -- data_out
            generic_tristate_controller_0_tcm_data_outen       : out   std_logic;                                        -- data_outen
            generic_tristate_controller_0_tcm_data_in          : in    std_logic_vector(7 downto 0)  := (others => 'X');  -- data_in

            uart_0_external_connection_rxd     : in    std_logic := 'X';            -- rxd
            uart_0_external_connection_txd     : out   std_logic                     -- txd
        );
    end component niosII_ps2;

begin

    -- Instanciacao do Sistema Nios II
    u0 : component niosII_ps2
        port map (
            clk_clk       => CLOCK_50,
            reset_reset_n => SW(0), -- Reset High
            botoes_export => KEY,
            ps2_CLK       => PS2_CLK,
            ps2_DAT       => PS2_DAT,

            -- Mapeamento dos pinos da Matriz
            leds_r1  => R1,
            leds_g1  => G1,
            leds_b1  => B1,
            leds_r2  => R2,
            leds_g2  => G2,
            leds_b2  => B2,
            leds_a   => A,
            leds_b   => B,
            leds_c   => C,
            leds_d   => D,
            leds_e   => E,
            leds_clk => CLK_OUT,
            leds_lat => LAT,
            leds_oe  => OE,

            sdram_addr  => DRAM_ADDR,
            sdram_ba    => DRAM_BA,
            sdram_cas_n => DRAM_CAS_N,
            sdram_cke   => DRAM_CKE,
            sdram_cs_n  => DRAM_CS_N,
            sdram_dq    => DRAM_DQ,
            sdram_dqm   => DRAM_DQM,
            sdram_ras_n => DRAM_RAS_N,
            sdram_we_n  => DRAM_WE_N,

            i2c_clk_external_connection_export => I2C_SCLK,
            i2c_sda_external_connection_export => I2C_SDAT,
				
				sd_spi_0_external_MISO             => SD_DAT0,             --           sd_spi_0_external.MISO
            sd_spi_0_external_MOSI             => SD_CMD,              --                            .MOSI
            sd_spi_0_external_SCLK             => SD_CLK,              --                            .SCLK
            sd_spi_0_external_SS_n             => SD_DAT3,             --                            .SS_n
				
					  -- Flash
            generic_tristate_controller_0_tcm_address_out      => FL_ADDR,
			   generic_tristate_controller_0_tcm_data_out         => flash_data_out,
			   generic_tristate_controller_0_tcm_data_in          => flash_data_in,
			   generic_tristate_controller_0_tcm_data_outen       => flash_data_outen,
			   generic_tristate_controller_0_tcm_chipselect_n_out => FL_CE_N,
			   generic_tristate_controller_0_tcm_read_n_out       => flash_read_n,
			   generic_tristate_controller_0_tcm_write_n_out      => FL_WE_N,
			   generic_tristate_controller_0_tcm_request          => flash_request,
			   generic_tristate_controller_0_tcm_grant            => flash_grant,

						
            uart_0_external_connection_rxd     => UART_RXD,
            uart_0_external_connection_txd     => UART_TXD
        );

    DRAM_CLK <= CLOCK_50;

    -- Flash em modo x8.
    FL_DQ <= flash_data_out when flash_data_outen = '1' else (others => 'Z');
    flash_data_in <= FL_DQ;
    FL_OE_N <= flash_read_n;
    flash_grant <= '1';
    FL_RST_N <= '1';
    FL_BYTE_N <= '0';
    FL_WP_N <= '1';
    FL_DQ15_AM1 <= '0';


end architecture;
