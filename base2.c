long base2(int power)
{
    if (power == 1)
        return (long)2;
    else if (power == 0)
        return 1;
    else if (power < 0)
        return (long)-1;
    else
        return (long)2 * base2(power - 1);
}
