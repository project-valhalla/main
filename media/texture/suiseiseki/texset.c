#include <stdio.h>

int main() {

	int i, n;
	FILE *f = fopen("texset.cfg", "w");

	printf("Insert: ");
	scanf("%i", &n);

	fprintf(f, "setshader bumpspecmapworld\n");
	fprintf(f, "\n");

	for(i = 0; i < n / 3; i++) {
		fprintf(f, "    texture c \"suiseiseki/%i.jpg\"\n", 3 * i + 1);
		fprintf(f, "    texture n \"suiseiseki/%i.jpg\"\n", 3 * i + 2);
		fprintf(f, "    texture s \"suiseiseki/%i.jpg\"\n", 3 * i + 3);
		fprintf(f, "\n");
	}

	return 0;

}
