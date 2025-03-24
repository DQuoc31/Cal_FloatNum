#include <iostream>
#include <bitset>
#define ull unsigned long long
using namespace std;

void dumpFloat(float* p) {
    // Chuyển đổi con trỏ float thành con trỏ unsigned int để thao tác trên bit
    unsigned int bits = *(reinterpret_cast<unsigned int*>(p));
    cout << bits << ' ';
    // Lấy bit dấu (bit thứ 31)
    unsigned int sign = (bits >> 31) & 1;

    // Lấy phần mũ (8 bit tiếp theo, từ bit 23 đến bit 30)
    unsigned int exponent = (bits >> 23) & 0xFF;

    // Lấy phần trị (23 bit cuối)
    unsigned int mantissa = bits & 0x7FFFFF;

    // In kết quả
    cout << sign << " ";
    cout << bitset<8>(exponent) << " ";
    cout << bitset<23>(mantissa) << '\n';
}

void forceFloat(float* p, const string& s) {
    unsigned int bits = 0;
    int len = s.length();

    // Ghi các bit từ chuỗi nhị phân vào số nguyên 32-bit
    for (int i = 0; i < len; i++) {
        if (s[i] == '1') {
            bits |= (1U << (31 - i));
        }
    }

    // Gán giá trị cho biến float
    *p = *(reinterpret_cast<float*>(&bits));
}

struct FloatBits {
    unsigned int sign;
    unsigned int exponent;
    unsigned int mantissa;
};

FloatBits floatToBits(float f) {
    unsigned int bits = *(reinterpret_cast<unsigned int*>(&f));
    return { (bits >> 31) & 1, (bits >> 23) & 0xFF, bits & 0x7FFFFF };
}

float bitsToFloat(FloatBits fb) {
    unsigned int bits = (fb.sign << 31) | (fb.exponent << 23) | fb.mantissa;
    return *(reinterpret_cast<float*>(&bits));
}

FloatBits addFloatBits(FloatBits a, FloatBits b) {
    if (a.exponent < b.exponent) swap(a, b);

    int shift = a.exponent - b.exponent;
    unsigned int mantissaA = (1 << 23) | a.mantissa;
    unsigned int mantissaB = ((1 << 23) | b.mantissa) >> shift;

    if (a.sign == b.sign) {
        // Cộng hai mantissa nếu cùng dấu
        unsigned int resultMantissa = mantissaA + mantissaB;

        if (resultMantissa & (1 << 24)) { // Kiểm tra tràn bit
            resultMantissa >>= 1;
            a.exponent++;
        }

        return { a.sign, a.exponent, resultMantissa & 0x7FFFFF };
    }
    else {
        // Nếu khác dấu, thực hiện phép trừ giữa số lớn hơn và số nhỏ hơn
        if (mantissaA < mantissaB) {
            swap(mantissaA, mantissaB);
            a.sign = b.sign;  // Dấu của kết quả là dấu của số lớn hơn
        }

        unsigned int resultMantissa = mantissaA - mantissaB;

        // Nếu kết quả là 0, trả về số 0
        if (resultMantissa == 0) return { 0, 0, 0 };

        // Chuẩn hóa kết quả (dịch trái để đưa về chuẩn IEEE 754)
        while (!(resultMantissa & (1 << 23))) {
            resultMantissa <<= 1;
            a.exponent--;
        }

        return { a.sign, a.exponent, resultMantissa & 0x7FFFFF };
    }
}


FloatBits subFloatBits(FloatBits a, FloatBits b) {
    if (a.sign == b.sign) {  // Nếu hai số cùng dấu, thực hiện phép trừ bình thường
        if (a.exponent < b.exponent || (a.exponent == b.exponent && a.mantissa < b.mantissa)) {
            swap(a, b);
            a.sign ^= 1;  // Đảo dấu kết quả nếu hoán đổi
        }

        int shift = a.exponent - b.exponent;
        unsigned int mantissaA = (1 << 23) | a.mantissa;
        unsigned int mantissaB = ((1 << 23) | b.mantissa) >> shift;

        unsigned int resultMantissa = mantissaA - mantissaB;

        // Chuẩn hóa lại kết quả
        while (resultMantissa && !(resultMantissa & (1 << 23))) {
            resultMantissa <<= 1;
            a.exponent--;
        }

        return { a.sign, a.exponent, resultMantissa & 0x7FFFFF };
    }
    else {
        // Nếu khác dấu, thực hiện phép cộng (x - (-y) = x + y)
        b.sign ^= 1;
        return addFloatBits(a, b);
    }
}


FloatBits mulFloatBits(FloatBits a, FloatBits b) {
    // Kiểm tra nhân với 0
    if ((a.exponent == 0 && a.mantissa == 0) || (b.exponent == 0 && b.mantissa == 0))
        return { 0, 0, 0 };  // Trả về 0

    // Tính toán dấu
    unsigned int signResult = a.sign ^ b.sign;

    // Lấy mantissa có bit ẩn
    unsigned long long mantissaA = (1ULL << 23) | a.mantissa;
    unsigned long long mantissaB = (1ULL << 23) | b.mantissa;

    // Nhân hai mantissa
    unsigned long long mantissaResult = (mantissaA * mantissaB);

    // Tính toán số mũ
    int exponentResult = a.exponent + b.exponent - 127;

    // Chuẩn hóa kết quả
    if (mantissaResult & (1ULL << 47)) {  // Nếu bit 47 bật, cần dịch phải 1 bit
        mantissaResult >>= 24;
        exponentResult++;
    }
    else {  // Nếu không, dịch phải 23 bit bình thường
        mantissaResult >>= 23;
    }

    // Kiểm tra overflow
    if (exponentResult >= 255) return { signResult, 255, 0 };  // Trả về +∞ hoặc -∞

    // Kiểm tra underflow
    if (exponentResult <= 0) return { signResult, 0, 0 };  // Trả về 0

    return { signResult, (unsigned int)exponentResult, (unsigned int)(mantissaResult & 0x7FFFFF) };
}


FloatBits divFloatBits(FloatBits a, FloatBits b) {
    // Kiểm tra chia cho 0
    if (b.exponent == 0 && b.mantissa == 0) {
        return { a.sign ^ b.sign, 255, 0 };  // Trả về +∞ hoặc -∞
    }

    // Chuẩn bị mantissa và thực hiện chia
    ull mantissaA = (1ULL << 23) | a.mantissa;
    ull mantissaB = (1ULL << 23) | b.mantissa;
    ull mantissaResult = (mantissaA << 23) / mantissaB;

    // Tính số mũ
    int exponentResult = a.exponent - b.exponent + 127;

    // Chuẩn hóa nếu cần
    while ((mantissaResult & (1 << 23)) == 0 && exponentResult > 0) {
        mantissaResult <<= 1;
        exponentResult--;
    }

    return { a.sign ^ b.sign, (unsigned int)exponentResult, (unsigned int)(mantissaResult & 0x7FFFFF) };
}


void printFloatBits(FloatBits fb) {
    cout << fb.sign << " " << bitset<8>(fb.exponent) << " " << bitset<23>(fb.mantissa) << " = " << bitsToFloat(fb) << '\n';
}

void Menu()
{
    int op;
    cout << "Danh sach cac lenh co the thuc hien: \n";
    cout << "1. Doi tu so thuc sang so cham dong.\n";
    cout << "2. Doi so cham dong sang so thuc.\n";
    cout << "3. Cong, tru, nhan, chia 2 so thuc (theo chuan IEEE 754).\n";
    cout << "0. Ket thuc chuong trinh.\n";
}

int main() {
    /*float x;
    cout << "Nhap vao so cham dong (32-bit): ";
    cin >> x;
    cout << "Bieu dien nhi phan tuong ung: ";
    dumpFloat(&x);

    string binary;
    cout << "Nhap day nhi phan: ";
    cin >> binary;

    if (binary.length() != 32) {
        cout << "Day nhi phan phai co dung 32 bit!" << endl;
        return 1;
    }

    forceFloat(&x, binary);

    cout << "So cham dong tuong ung: " << x << endl;
    float x;

    // Khảo sát số 1.3E+20
    x = 1.3E+20;
    cout << "1.3E+20 co bieu dien nhi phan: ";
    dumpFloat(&x);

    // Số float nhỏ nhất lớn hơn 0
    x = nextafter(0.0f, 1.0f);
    cout << "So thuc lon nhat nho hon 0: " << x << endl;
    cout << "Biểu diễn nhị phân: ";
    dumpFloat(&x);

    // Các trường hợp số đặc biệt
    float inf = INFINITY;
    float neg_inf = -INFINITY;
    float nan_val = NAN;

    cout << "Số vô cùng dương: ";
    dumpFloat(&inf);
    cout << "Số vô cùng âm: ";
    dumpFloat(&neg_inf);
    cout << "NaN: ";
    dumpFloat(&nan_val);

    */

    float a, b;
    cout << "Nhap so thuc a: ";
    cin >> a;
    cout << "Nhap so thuc b: ";
    cin >> b;

    FloatBits num1 = floatToBits(a);
    FloatBits num2 = floatToBits(b);

    cout << "Phép cộng: ";
    printFloatBits(addFloatBits(num1, num2));

    cout << "Phép trừ: ";
    printFloatBits(subFloatBits(num1, num2));

    cout << "Phép nhân: ";
    printFloatBits(mulFloatBits(num1, num2));

    cout << "Phép chia: ";
    printFloatBits(divFloatBits(num1, num2));

    return 0;
}
