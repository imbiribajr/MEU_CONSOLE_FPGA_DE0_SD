library ieee;
use ieee.std_logic_1164.all;

entity Hardware is
    port (
        CLOCK_50 : in  std_logic;
        CLOCK_27 : in  std_logic;
        KEY      : in  std_logic_vector(3 downto 0);   -- Botões da DE2
		SW		 : in  std_logic_vector(16 downto 0); -- Chaves da DE2
		  
		PS2_CLK  : inout std_logic;
        PS2_DAT  : inout std_logic;

        -- Pinos da Matriz de LED (HUB75)
        R1, G1, B1 : out std_logic;
        R2, G2, B2 : out std_logic;
        A, B, C, D, E : out std_logic;
        CLK_OUT : out std_logic;
        LAT     : out std_logic;
        OE      : out std_logic;
		  
		DRAM_ADDR     : out   std_logic_vector(11 downto 0);                    -- addr
        DRAM_BA       : out   std_logic_vector(1 downto 0);                     -- ba
        DRAM_CAS_N    : out   std_logic;                                        -- cas_n
        DRAM_CKE      : out   std_logic;                                        -- cke
        DRAM_CS_N     : out   std_logic;                                        -- cs_n
        DRAM_DQ       : inout std_logic_vector(15 downto 0) := (others => 'X'); -- dq
        DRAM_DQM      : out   std_logic_vector(1 downto 0);                     -- dqm
        DRAM_RAS_N    : out   std_logic;                                        -- ras_n
        DRAM_WE_N     : out   std_logic;                                         -- we_n	 		 
		DRAM_CLK 	  : out   std_logic;

        -- Pinos da Matriz de Audio
        I2C_SDAT		 : inout std_logic;
		I2C_SCLK		 : out   std_logic;
		  
		AUD_ADCDAT 	 : in    std_logic;
		AUD_ADCLRCK   : in    std_logic;
		AUD_DACLRCK   : in    std_logic;
		AUD_DACDAT    : out   std_logic;
		AUD_BCLK      : in    std_logic;
		AUD_XCK       : out   std_logic
    );
end entity;

architecture rtl of Hardware is

    signal clk_audio : std_logic;

    component Audio_PLL is
        port (
            areset : in  std_logic := 'X';
            inclk0 : in  std_logic := 'X';
            c0     : out std_logic
        );
    end component Audio_PLL;


    -- Declaração do componente gerado pelo Qsys (Nome: niosII)
	component niosII_ps2 is
        port (
            clk_clk       : in    std_logic                     := 'X';             -- clk
            botoes_export : in    std_logic_vector(3 downto 0)  := (others => 'X'); -- export
            clk_audio_clk_in_clk : in    std_logic                     := 'X';             -- clk
            
            leds_G1       : out   std_logic;                                        -- G1 --G1
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
            reset_reset_n : in    std_logic                     := 'X';             -- reset_n
            ps2_CLK       : inout std_logic                     := 'X';             -- CLK
            ps2_DAT       : inout std_logic                     := 'X';             -- DAT
            sdram_addr    : out   std_logic_vector(11 downto 0);                    -- addr
            sdram_ba      : out   std_logic_vector(1 downto 0);                     -- ba
            sdram_cas_n   : out   std_logic;                                        -- cas_n
            sdram_cke     : out   std_logic;                                        -- cke
            sdram_cs_n    : out   std_logic;                                        -- cs_n
            sdram_dq      : inout std_logic_vector(15 downto 0) := (others => 'X'); -- dq
            sdram_dqm     : out   std_logic_vector(1 downto 0);                     -- dqm
            sdram_ras_n   : out   std_logic;                                        -- ras_n
            sdram_we_n    : out   std_logic;                                         -- we_n
            
            -- Audio
            
            i2c_clk_external_connection_export : out   std_logic;                     -- export
            i2c_sda_external_connection_export : inout std_logic  := 'X';             -- export
            audio_conduit_end_XCK              : out   std_logic;                     -- XCK
            audio_conduit_end_ADCDAT           : in    std_logic  := 'X';             -- ADCDAT
            audio_conduit_end_ADCLRC           : in    std_logic  := 'X';             -- ADCLRC
            audio_conduit_end_DACDAT           : out   std_logic;                     -- DACDAT
            audio_conduit_end_DACLRC           : in    std_logic  := 'X';             -- DACLRC
            audio_conduit_end_BCLK             : in    std_logic  := 'X'              -- BCLK


        );
    end component niosII_ps2;


begin

    Audio_PLL_inst : component Audio_PLL
        port map (
            areset => '0',
            inclk0 => CLOCK_27,
            c0     => clk_audio
        );

    -- Instanciação do Sistema Nios II
    u0 : component niosII_ps2
        port map (
            clk_clk       => CLOCK_50,
            clk_audio_clk_in_clk => clk_audio,
            reset_reset_n => SW(0),           -- Reset High)
            botoes_export => KEY,            -- Liga as chaves da DE2
		
			ps2_CLK   => PS2_CLK,
            ps2_DAT   => PS2_DAT,
            
            -- Mapeamento dos pinos da Matriz
            leds_r1   => R1,
            leds_g1   => G1,
            leds_b1   => B1,
            leds_r2   => R2,
            leds_g2   => G2,
            leds_b2   => B2,
            leds_a    => A,
            leds_b    => B,
            leds_c    => C,
            leds_d    => D,
            leds_e    => E,
            leds_clk  => CLK_OUT,
            leds_lat  => LAT,
            leds_oe   => OE,
				
			sdram_addr    => DRAM_ADDR,      
            sdram_ba      => DRAM_BA,   
            sdram_cas_n   => DRAM_CAS_N,
            sdram_cke     => DRAM_CKE,  
            sdram_cs_n    => DRAM_CS_N, 
            sdram_dq      => DRAM_DQ,   
            sdram_dqm     => DRAM_DQM,  
            sdram_ras_n   => DRAM_RAS_N,
            sdram_we_n    => DRAM_WE_N,

            i2c_clk_external_connection_export => I2C_SCLK,             -- i2c_clk_external_connection.export
            i2c_sda_external_connection_export => I2C_SDAT,             -- i2c_sda_external_connection.export
            audio_conduit_end_XCK              => AUD_XCK,              -- audio_conduit_end.XCK
            audio_conduit_end_ADCDAT           => AUD_ADCDAT,           -- .ADCDAT
            audio_conduit_end_ADCLRC           => AUD_ADCLRCK,          -- .ADCLRC
            audio_conduit_end_DACDAT           => AUD_DACDAT,           -- .DACDAT
            audio_conduit_end_DACLRC           => AUD_DACLRCK,          -- .DACLRC
            audio_conduit_end_BCLK             => AUD_BCLK              -- .BCLK
        );
		  
		  
		  DRAM_CLK <= CLOCK_50;

end architecture;