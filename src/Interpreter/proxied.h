class Proxied : public Interpreter {
   public:
    bool store(Module *m, uint8_t type, uint32_t addr,
               StackValue &sval) override;
};