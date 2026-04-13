library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ps2_avalon_interface is
    port (
        clk          : in    std_logic;
        reset_n      : in    std_logic;

        -- Avalon-MM (read-only slave)
        avs_read     : in    std_logic;
        avs_readdata : out   std_logic_vector(31 downto 0);

        -- PS/2 pins
        PS2_CLK      : inout std_logic;
        PS2_DAT      : inout std_logic
    );
end entity;

architecture rtl of ps2_avalon_interface is

    component ps2_keyboard is
        port (
            clk, reset : in std_logic;

            ps2_clk_en_o, ps2_data_en_o : out std_logic;
            ps2_clk_i, ps2_data_i       : in  std_logic;

            rx_scan_code  : out std_logic_vector(7 downto 0);
            rx_data_ready : out std_logic;
            rx_read       : in  std_logic;

            -- GARANTIR QUE ESSAS SAÍDAS SÃO std_logic DE 1 BIT
            rx_extended : out std_logic; 
            rx_released : out std_logic; 
            rx_shift_key_on : out std_logic; -- (Mudei aqui, era std_logic_vector(0 downto 0))
            rx_ascii : out std_logic_vector(7 downto 0);

            tx_data  : in std_logic_vector(7 downto 0);
            tx_write : in std_logic;
            translate: in std_logic;

            tx_write_ack_o, tx_error_no_keyboard_ack : out std_logic
        );
    end component;

    signal reset_high : std_logic;

    -- PS/2 IO
    signal ps2_clk_in, ps2_dat_in : std_logic;
    signal ps2_clk_en, ps2_dat_en : std_logic;

    -- PS/2 core
    signal scan_code_wire  : std_logic_vector(7 downto 0);
    signal data_ready_wire : std_logic;
    signal rx_read_pulse   : std_logic := '0';

    -- Sinais para capturar flags do Verilog (std_logic de 1 bit)
    signal released_flag   : std_logic := '0'; -- Captura o rx_released do Verilog
    signal extended_flag   : std_logic := '0'; -- Captura o rx_extended do Verilog
    
    -- FIFO 4 entries: [9]=extended_flag, [8]=released_flag, [7:0]=code
    type fifo_t is array (0 to 3) of std_logic_vector(9 downto 0);
    signal fifo   : fifo_t := (others => (others => '0'));
    signal rd_ptr : unsigned(1 downto 0) := (others => '0');
    signal wr_ptr : unsigned(1 downto 0) := (others => '0');
    signal count  : unsigned(2 downto 0) := (others => '0'); -- 0..4

    signal valid : std_logic := '0';

    function inc2(x : unsigned(1 downto 0)) return unsigned is
    begin
        return x + 1;
    end function;

begin
    reset_high <= not reset_n;
    valid <= '1' when (count /= 0) else '0';

    ----------------------------------------------------------------
    -- PS/2 keyboard core (translate=1: E0/F0 viram flags)
    ----------------------------------------------------------------
    u0 : ps2_keyboard
        port map (
            clk   => clk,
            reset => reset_high,

            ps2_clk_i     => ps2_clk_in,
            ps2_data_i    => ps2_dat_in,
            ps2_clk_en_o  => ps2_clk_en,
            ps2_data_en_o => ps2_dat_en,

            rx_scan_code  => scan_code_wire,
            rx_data_ready => data_ready_wire,
            rx_read       => rx_read_pulse,

            -- LIGAR DIRETAMENTE AS FLAGS DO VERILOG
            rx_extended => extended_flag, -- <--- AQUI
            rx_released => released_flag, -- <--- AQUI
            rx_shift_key_on => open,
            rx_ascii => open,

            tx_data    => (others => '0'),
            tx_write   => '0',
            translate  => '1',

            tx_write_ack_o => open,
            tx_error_no_keyboard_ack => open
        );

    ----------------------------------------------------------------
    -- PS/2 tristate
    ----------------------------------------------------------------
    ps2_clk_in <= PS2_CLK;
    ps2_dat_in <= PS2_DAT;

    PS2_CLK <= '0' when ps2_clk_en = '0' else 'Z';
    PS2_DAT <= '0' when ps2_dat_en = '0' else 'Z';

    ----------------------------------------------------------------
    -- FIFO + Avalon (read-to-pop)
    ----------------------------------------------------------------
    process(clk)
        variable fifo_full  : boolean;
        variable fifo_empty : boolean;
    begin
        if rising_edge(clk) then
            rx_read_pulse <= '0';

            if reset_n = '0' then
                fifo       <= (others => (others => '0'));
                rd_ptr     <= (others => '0');
                wr_ptr     <= (others => '0');
                count      <= (others => '0');
            else
                fifo_full  := (count = 4);
                fifo_empty := (count = 0);

                -- PUSH: enfileira {extended_flag, released_flag, code}
                if (data_ready_wire = '1') and (not fifo_full) then
                    -- ATRIBUIÇÃO CORRIGIDA DOS BITS
                    fifo(to_integer(wr_ptr)) <= extended_flag & released_flag & scan_code_wire;
                    wr_ptr <= inc2(wr_ptr);
                    count  <= count + 1;
                    rx_read_pulse <= '1';
                end if;

                -- POP: 1 read consome 1 entrada
                if (avs_read = '1') and (not fifo_empty) then -- AVS_READ é a condição
                    rd_ptr <= inc2(rd_ptr);
                    count  <= count - 1;
                end if;
            end if;
        end if;
    end process;

    ----------------------------------------------------------------
    -- Avalon readdata: CONTRATO DEFINITIVO
    -- bit31 = VALID
    -- bit30 = RELEASED
    -- bit29 = EXTENDED
    -- bits10..8 = COUNT
    -- bits7..0 = SCAN_CODE
    ----------------------------------------------------------------
    avs_readdata <=
        valid &                                -- bit31
        fifo(to_integer(rd_ptr))(8) &          -- bit30 (Released)
        fifo(to_integer(rd_ptr))(9) &          -- bit29 (Extended)
        "000000000000000000" &                 -- 18 zeros
        std_logic_vector(count) &              -- bits10..8 (Count da FIFO)
        fifo(to_integer(rd_ptr))(7 downto 0);  -- bits7..0 (Scan Code)

end architecture;