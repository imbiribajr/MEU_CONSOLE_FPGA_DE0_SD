library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library altera_mf;
use altera_mf.altera_mf_components.all;

entity led_matrix_avalon is
    generic (
        WIDTH_PIXELS  : integer := 128;
        HEIGHT_PIXELS : integer := 64
    );
    port (
        clk        : in  std_logic;
        reset_n    : in  std_logic;
        avs_address     : in  std_logic_vector(13 downto 0);
        avs_writedata   : in  std_logic_vector(7 downto 0);
        avs_write       : in  std_logic;
        avs_readdata    : out std_logic_vector(7 downto 0);
        avs_read        : in  std_logic;

        HUB_R1, HUB_G1, HUB_B1 : out std_logic;
        HUB_R2, HUB_G2, HUB_B2 : out std_logic;
        HUB_A, HUB_B, HUB_C, HUB_D, HUB_E : out std_logic;
        HUB_CLK, HUB_LAT, HUB_OE : out std_logic
    );
end entity;

architecture rtl of led_matrix_avalon is

    signal addr_int : integer range 0 to 16383;
    signal addr_upper_int : integer range 0 to 4095 := 0;
    
    signal we_upper      : std_logic;
    signal q_upper_nios  : std_logic_vector(2 downto 0);
    signal q_upper_video : std_logic_vector(2 downto 0);
    signal addr_upper_video : std_logic_vector(11 downto 0);
    signal addr_upper_nios  : std_logic_vector(11 downto 0);

    signal we_lower      : std_logic;
    signal q_lower_nios  : std_logic_vector(2 downto 0);
    signal q_lower_video : std_logic_vector(2 downto 0);
    signal addr_lower_video : std_logic_vector(11 downto 0);
    signal addr_lower_nios  : std_logic_vector(11 downto 0);
    signal addr_lower_int   : integer range 0 to 4095 := 0;

    constant ADDR_OFFSET : integer := 4095; -- corrige wrap por metade (addr-1)

    function wrap4096(v : integer) return std_logic_vector is
        variable t : integer;
    begin
        t := v;
        if t >= 4096 then
            t := t - 4096;
        elsif t < 0 then
            t := t + 4096;
        end if;
        return std_logic_vector(to_unsigned(t, 12));
    end function;

    signal col_cnt   : integer range 0 to WIDTH_PIXELS-1 := 0;
    signal row_sel   : integer range 0 to 31 := 0;
    signal state_drv : integer range 0 to 6 := 0;
    signal oe_sig, lat_sig, clk_sig : std_logic := '0';
	 
	 -- NOVO SINAL: Contador de brilho (PWM)
    -- Aumente o valor 2500 se quiser mais brilho (cuidado com flickering)
    -- Diminua se quiser menos brilho

	 constant BRIGHT_ADDR : integer := 8192;  -- 0..8191 = pixels
	 signal is_bright_write : std_logic;
	 
	 signal brightness_cpu : integer range 0 to 255 := 100;
	 signal brightness_reg : integer range 0 to 255 := 100;
	 signal pwm_cnt        : integer range 0 to 255 := 0;
	 
	 -- Síncronização
	 signal 	 frame_done      : std_logic := '0';
	 constant STATUS_ADDR     : integer := 8193;
    signal   is_status_write : std_logic;

begin

    addr_int <= to_integer(unsigned(avs_address));
    addr_upper_int <= addr_int when addr_int < 4096 else 0;
    addr_lower_int <= addr_int - 4096 when addr_int >= 4096 else 0;
    addr_upper_nios <= wrap4096(addr_upper_int + ADDR_OFFSET);
    addr_lower_nios <= wrap4096(addr_lower_int + ADDR_OFFSET);
	 
	 is_bright_write <= '1' when (avs_write='1' and addr_int = BRIGHT_ADDR) else '0';
	 
	 is_status_write <= '1' when (avs_write='1' and addr_int = STATUS_ADDR) else '0';

    --========================================================================
    -- RAM SUPERIOR (Linhas 0-31)
    -- Modo BIDIR_DUAL_PORT permite leitura/escrita independente em A e B
    --========================================================================
    we_upper <= '1' when (avs_write='1' and addr_int < 4096 and is_bright_write='0') else '0';

    ram_upper_inst : altsyncram
    generic map (
        operation_mode => "BIDIR_DUAL_PORT", -- CORREÇÃO: Modo True Dual Port
        width_a => 3, widthad_a => 12,
        numwords_a => 4096,
        width_b => 3, widthad_b => 12,
        numwords_b => 4096,
        lpm_type => "altsyncram",
        outdata_reg_a => "UNREGISTERED",     -- CORREÇÃO: Saída A habilitada
        outdata_reg_b => "UNREGISTERED",
        indata_aclr_a => "NONE", wrcontrol_aclr_a => "NONE", address_aclr_a => "NONE",
        address_aclr_b => "NONE", outdata_aclr_b => "NONE", outdata_aclr_a => "NONE",
        read_during_write_mode_mixed_ports => "DONT_CARE"
    )
    port map (
        clock0    => clk,
        clock1    => clk,
        
        -- Porta A: Nios II
        address_a => addr_upper_nios,
        data_a    => avs_writedata(2 downto 0),
        wren_a    => we_upper,
        q_a       => q_upper_nios,
        
        -- Porta B: Driver de Vídeo
        address_b => addr_upper_video,
        data_b    => (others => '0'),
        wren_b    => '0',
        q_b       => q_upper_video
    );

    --========================================================================
    -- RAM INFERIOR (Linhas 32-63)
    --========================================================================
    we_lower <= '1' when (avs_write='1' and addr_int >= 4096 and addr_int < 8192 and is_bright_write='0') else '0';

    ram_lower_inst : altsyncram
    generic map (
        operation_mode => "BIDIR_DUAL_PORT", -- CORREÇÃO
        width_a => 3, widthad_a => 12,
        numwords_a => 4096,
        width_b => 3, widthad_b => 12,
        numwords_b => 4096,
        lpm_type => "altsyncram",
        outdata_reg_a => "UNREGISTERED",     -- CORREÇÃO
        outdata_reg_b => "UNREGISTERED",
        indata_aclr_a => "NONE", wrcontrol_aclr_a => "NONE", address_aclr_a => "NONE",
        address_aclr_b => "NONE", outdata_aclr_b => "NONE", outdata_aclr_a => "NONE",
        read_during_write_mode_mixed_ports => "DONT_CARE"
    )
    port map (
        clock0    => clk,
        clock1    => clk,
        
        address_a => addr_lower_nios,
        data_a    => avs_writedata(2 downto 0),
        wren_a    => we_lower,
        q_a       => q_lower_nios,
        
        address_b => addr_lower_video,
        data_b    => (others => '0'),
        wren_b    => '0',
        q_b       => q_lower_video
    );

    -- Leitura para o Nios
    process(clk) begin
        if rising_edge(clk) then
            if avs_read = '1' then
					if addr_int = STATUS_ADDR then
						  avs_readdata <= "0000000" & frame_done;
					elsif addr_int < 4096 then
						  avs_readdata <= "00000" & q_upper_nios;
					else
						  avs_readdata <= "00000" & q_lower_nios;
					end if;
				end if;

        end if;
    end process;
	 
	 -- NOVO PROCESSO: Escrita no Registrador de Brilho
    -- Processo de Escrita do Brilho (Endereço 8191)
    process(clk)
		begin
		  if rising_edge(clk) then
			 if reset_n = '0' then
				brightness_cpu <= 100;
			 elsif is_bright_write = '1' then
				brightness_cpu <= to_integer(unsigned(avs_writedata)); -- 0..255
			 end if;
		  end if;
	end process;
	
    --========================================================================
    -- Driver HUB75
    --========================================================================
    process(clk)
        variable addr_calc : integer;
    begin
        if rising_edge(clk) then
            if reset_n = '0' then
                state_drv <= 0; col_cnt <= 0;
					 frame_done <= '0';
            else
						-- clear por escrita da CPU (mesmo clock)
					if is_status_write = '1' then
                frame_done <= '0';
					end if;
					
						case state_drv is
                    when 0 => 
                        clk_sig <= '0';
                        addr_calc := (row_sel * WIDTH_PIXELS) + col_cnt;
                        addr_upper_video <= std_logic_vector(to_unsigned(addr_calc, 12));
                        addr_lower_video <= std_logic_vector(to_unsigned(addr_calc, 12));
                        state_drv <= 1;

                    when 1 => 
                        HUB_R1 <= q_upper_video(0); HUB_G1 <= q_upper_video(1); HUB_B1 <= q_upper_video(2);
                        HUB_R2 <= q_lower_video(0); HUB_G2 <= q_lower_video(1); HUB_B2 <= q_lower_video(2);
                        clk_sig <= '1';
                        state_drv <= 2;

                    when 2 => 
                        clk_sig <= '0';
                        if col_cnt = WIDTH_PIXELS-1 then col_cnt <= 0; state_drv <= 3;
                        else col_cnt <= col_cnt + 1; state_drv <= 0; end if;

                    when 3 => 
                        oe_sig <= '1'; lat_sig <= '1'; state_drv <= 4;

                     -- ESTADO 4: Latch
                    when 4 => 
                        lat_sig <= '1';
								oe_sig  <= '1';

								pwm_cnt <= 0;
								brightness_reg <= brightness_cpu;  -- <<< LATCH SEGURO

								state_drv <= 5;
                    
						  -- ESTADO 5: Enable Display (Acende os LEDs com Duração Controlada)
                    when 5 =>
								 lat_sig <= '0';
								 oe_sig  <= '0';

								 if pwm_cnt < brightness_reg then
									  pwm_cnt <= pwm_cnt + 1;
								 else
									  state_drv <= 6;
								 end if;

                    -- ESTADO 6: Troca de Linha
                    when 6 =>
                        oe_sig  <= '1'; -- Apaga o display
                        
                        if row_sel = 31 then 
                            row_sel <= 0; 
									 frame_done <= '1';  -- fim de frame
                        else 
                            row_sel <= row_sel + 1; 
                        end if;
                        
                        state_drv <= 0;
                end case;
            end if;
        end if;
    end process;

    HUB_CLK <= clk_sig; HUB_LAT <= lat_sig; HUB_OE  <= oe_sig;
    HUB_A <= '1' when (row_sel mod 2)/=0 else '0'; HUB_B <= '1' when (row_sel/2) mod 2/=0 else '0';
    HUB_C <= '1' when (row_sel/4) mod 2/=0 else '0'; HUB_D <= '1' when (row_sel/8) mod 2/=0 else '0';
    HUB_E <= '1' when (row_sel/16)>0 else '0';

end architecture;
