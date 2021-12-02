
/**********************************************************
 *
 *        M u l t i   C a m e r a   S i m u l a t o r
 *
 * License: proprietary
 *
 * Copyrights: Marcelo Varanda
 * Initial date: October 2021
 * http://www.varanda.ca
 *
 ***********************************************************/

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
